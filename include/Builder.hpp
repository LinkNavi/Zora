#pragma once
#include "Zora.hpp"
#include "Workspace.hpp"
#include "Glob.hpp"
#include "Incremental.hpp"
#include "Compiler.hpp"
#include "ErrorFormatter.hpp"
#include "Logger.hpp"
#include "DepManager.hpp"
#include "ProcessRun.hpp"
#include "HylianBuilder.hpp"
#include <array>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace Zora {

namespace fs = std::filesystem;

static std::mutex g_print_mutex;

struct CompileJob {
    fs::path src;
    fs::path obj;
    fs::path dep;
    fs::path rel;
};

// per-project accumulation of JSON compilation-database entries, keyed by
// project root; written out to <root>/compile_commands.json once the whole
// workspace build finishes so LSPs (clangd etc) can pick up Zora's flags.
using CompileDb = std::map<fs::path, std::vector<std::string>>;

inline std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

inline void write_compile_db(const CompileDb& cdb) {
    for (auto& [root, entries] : cdb) {
        if (entries.empty()) continue;
        std::ofstream f(root / "compile_commands.json");
        f << "[\n";
        for (size_t i = 0; i < entries.size(); i++)
            f << entries[i] << (i + 1 < entries.size() ? ",\n" : "\n");
        f << "]\n";
    }
}

inline std::string build_pch(const fs::path& build_dir, const fs::path& proj_root,
                               const Target& target, const std::string& compiler,
                               const std::string& inc_flags, const std::string& extra_flags) {
    fs::path pch_header = build_dir / "zora_pch.hpp";
    fs::path pch_out    = build_dir / "zora_pch.hpp.pch";

    if (fs::exists(pch_out)) {
        auto pch_time = mtime(pch_out);
        bool stale = false;
        for (auto& d : target.include_dirs) {
            fs::path abs_d = fs::absolute(proj_root / d);
            if (!fs::exists(abs_d)) continue;
            for (auto& entry : fs::recursive_directory_iterator(abs_d)) {
                if (entry.is_regular_file() && mtime(entry.path()) > pch_time) {
                    stale = true; break;
                }
            }
            if (stale) break;
        }
        if (!stale) { log_verbose("PCH up to date"); return pch_out.string(); }
    }

    {
        std::FILE* f = std::fopen(pch_header.string().c_str(), "w");
        if (!f) return "";
        for (auto& d : target.include_dirs) {
            fs::path abs_d = fs::absolute(proj_root / d);
            if (!fs::exists(abs_d)) continue;
            for (auto& entry : fs::recursive_directory_iterator(abs_d))
                if (entry.path().extension() == ".hpp" || entry.path().extension() == ".h")
                    std::fprintf(f, "#include \"%s\"\n", entry.path().string().c_str());
        }
        std::fclose(f);
    }

    std::printf("  %sPrecompiling%s headers\n", Color::Cyan, Color::Reset);
    std::string cmd = compiler + " -std=c++23 -x c++-header"
        + inc_flags + extra_flags
        + " " + pch_header.string()
        + " -o " + pch_out.string();
    log_debug("pch: " + cmd);

    std::string err;
    int rc = run_capture(cmd, err);
    if (!err.empty()) format_stderr(err);
    if (rc != 0) return "";
    return pch_out.string();
}

inline bool build_target(const WorkspaceProject& proj, const Target& target,
                          const std::string& compiler, CompileDb& cdb) {
    fs::path build_dir = proj.root / "zora-build" / target.name;
    fs::path bin_dir   = proj.root / "zora-build" / "bin";
    fs::create_directories(build_dir);
    fs::create_directories(bin_dir);

    fs::path out_path = (target.type == "staticlib")
        ? bin_dir / (target.name + ".a")
        : bin_dir / target.name;

    auto sources = glob_all(proj.root, target.sources);
    if (sources.empty()) {
        log_err("No sources found for target: " + target.name);
        return false;
    }

    // resolve deps first — clones, builds, injects flags
    std::string dep_inc, dep_lib;
    if (!resolve_deps(target, proj.root, dep_inc, dep_lib)) return false;

    std::string inc_flags;
    for (auto& d : target.include_dirs)
        inc_flags += " -I" + (proj.root / d).string();
    inc_flags += dep_inc;

    std::string extra_flags;
    for (auto& f : target.compiler_flags)
        extra_flags += " " + f;

    std::string speed_flags = " -pipe";

    std::string linker_flags = dep_lib;
    if (std::system("ld.lld --version > /dev/null 2>&1") == 0)
        linker_flags += " -fuse-ld=lld";

    std::string pch_flag;
    std::string pch_path = build_pch(build_dir, proj.root, target, compiler,
                                      inc_flags, extra_flags + speed_flags);
    if (!pch_path.empty())
        pch_flag = " -include-pch " + pch_path;

    auto make_cmd = [&](const fs::path& src, const fs::path& obj, const fs::path& dep) {
        return compiler + " -std=c++23"
            + speed_flags + inc_flags + extra_flags + pch_flag
            + " -MMD -MF " + dep.string()
            + " -c " + src.string()
            + " -o " + obj.string();
    };

    // same as make_cmd but without -include-pch: clangd (and other LSPs) run
    // their own bundled clang, which may not agree with the PCH format the
    // real build compiler emits — an incompatible -include-pch can make
    // clangd fail to parse the file at all, so leave it out of the db entry.
    auto make_cdb_cmd = [&](const fs::path& src, const fs::path& obj, const fs::path& dep) {
        return compiler + " -std=c++23"
            + speed_flags + inc_flags + extra_flags
            + " -MMD -MF " + dep.string()
            + " -c " + src.string()
            + " -o " + obj.string();
    };

    std::vector<CompileJob> jobs;
    std::vector<fs::path>   objects;
    auto& cdb_entries = cdb[proj.root];

    for (auto& src : sources) {
        fs::path rel = fs::relative(src, proj.root);
        fs::path obj = build_dir / (rel.string() + ".o");
        fs::path dep = build_dir / (rel.string() + ".d");
        fs::create_directories(obj.parent_path());
        objects.push_back(obj);
        if (needs_rebuild(src, obj, dep))
            jobs.push_back({src, obj, dep, rel});

        cdb_entries.push_back(
            "  {\n"
            "    \"directory\": \"" + json_escape(proj.root.string()) + "\",\n"
            "    \"file\": \""      + json_escape(src.string())       + "\",\n"
            "    \"command\": \""  + json_escape(make_cdb_cmd(src, obj, dep)) + "\",\n"
            "    \"output\": \""   + json_escape(obj.string())        + "\"\n"
            "  }");
    }

    // A local/workspace path dep (e.g. Runtime -> ../Engine) can change
    // without touching any of *this* target's own sources, so object
    // staleness alone isn't enough — if the dep's lib got rebuilt more
    // recently than our own output, we're linked against a stale copy.
    bool dep_lib_newer = false;
    if (fs::exists(out_path)) {
        auto out_time = mtime(out_path);
        for (auto& dep : target.deps) {
            if (dep.path.empty()) continue;
            fs::path dep_lib = fs::absolute(proj.root / dep.path) / "zora-build" / "bin" / (dep.name + ".a");
            if (fs::exists(dep_lib) && mtime(dep_lib) > out_time) { dep_lib_newer = true; break; }
        }
    }

    // objects being up to date isn't enough — if a previous run compiled
    // everything but got interrupted (or failed) before linking, the final
    // output can still be missing even though every .o looks current.
    if (jobs.empty() && !dep_lib_newer && fs::exists(out_path)) {
        std::printf("  %sUp to date%s %s\n", Color::Blue, Color::Reset, target.name.c_str());
        return true;
    }

    unsigned int n_threads = std::max(1u, std::thread::hardware_concurrency());
    if (!jobs.empty()) log_info("Compiling with " + std::to_string(n_threads) + " threads");

    std::atomic<bool> any_error{false};
    std::atomic<int>  job_idx{0};

    auto worker = [&]() {
        while (true) {
            int idx = job_idx.fetch_add(1, std::memory_order_relaxed);
            if (idx >= (int)jobs.size()) break;
            auto& job = jobs[idx];

            {
                std::lock_guard<std::mutex> lk(g_print_mutex);
                std::printf("  %sCompiling%s %s\n",
                    Color::Cyan, Color::Reset, job.rel.c_str());
            }

            std::string cmd = make_cmd(job.src, job.obj, job.dep);

            log_debug("cmd: " + cmd);

            std::string err_out;
            int rc = run_capture(cmd, err_out);
            if (!err_out.empty()) {
                std::lock_guard<std::mutex> lk(g_print_mutex);
                format_stderr(err_out);
            }
            if (rc != 0) any_error.store(true, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (unsigned i = 0; i < n_threads; i++)
        threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    if (any_error) return false;

    std::printf("  %sLinking%s   %s\n", Color::White, Color::Reset, target.name.c_str());

    std::string obj_list;
    for (auto& o : objects) obj_list += " " + o.string();

    std::string link_cmd;
    if (target.type == "staticlib") {
        link_cmd = "ar rcs " + out_path.string() + obj_list;
    } else {
        link_cmd = compiler + " -std=c++23" + linker_flags + " " + obj_list + " -o " + out_path.string();
    }

    log_debug("link: " + link_cmd);

    std::string link_err;
    int rc = run_capture(link_cmd, link_err);
    if (!link_err.empty()) format_stderr(link_err);
    if (rc != 0) return false;

    std::printf("  %sFinished%s  %s\n", Color::Bold, Color::Reset, target.name.c_str());
    return true;
}

// resolve_deps() for a local (`path=`) dep only checks that the dependency's
// .a already *exists* on disk — it has no idea whether the sources that
// produced it are still current, so a target built via `zora run` used to
// silently link against a stale copy if its dep hadn't been rebuilt since
// the last `zora build`. This walks `target`'s local workspace deps
// (recursively, so a dep-of-a-dep like Runtime -> Engine -> ECS gets
// refreshed too) and rebuilds each one — through the same incremental
// build_target() everything else goes through, so already-current deps are
// a cheap no-op, not a full rebuild every time.
inline bool build_dependency_closure_impl(const Workspace& ws, const WorkspaceProject& proj,
                                           const Target& target, const std::string& compiler,
                                           CompileDb& cdb, std::set<std::string>& visited) {
    const auto& projects = ws.is_workspace
        ? ws.members
        : std::vector<WorkspaceProject>{ws.root_project};

    bool ok = true;
    for (auto& dep : target.deps) {
        if (dep.path.empty()) continue; // git/system deps are handled by resolve_deps itself

        std::error_code ec;
        fs::path dep_root = fs::weakly_canonical(fs::absolute(proj.root / dep.path), ec);
        if (ec) continue;

        const WorkspaceProject* dep_proj = nullptr;
        for (auto& p : projects) {
            fs::path p_root = fs::weakly_canonical(p.root, ec);
            if (!ec && p_root == dep_root) { dep_proj = &p; break; }
        }
        if (!dep_proj) continue; // dep lives outside this workspace — nothing we can rebuild here

        const Target* dep_target = nullptr;
        for (auto& t : dep_proj->file.targets)
            if (t.name == dep.name) { dep_target = &t; break; }
        if (!dep_target) continue;

        std::string key = dep_root.string() + ":" + dep_target->name;
        if (!visited.insert(key).second) continue; // already handled earlier in this run (diamond dep)

        // Depend-on-a-dependency first (post-order), then this one.
        if (!build_dependency_closure_impl(ws, *dep_proj, *dep_target, compiler, cdb, visited))
            ok = false;

        if (is_hylian_target(*dep_target)) {
            if (!build_hylian_target(*dep_proj, *dep_target)) ok = false;
        } else {
            std::printf("\n%sBuilding%s %s\n", Color::Bold, Color::Reset, dep_proj->file.name.c_str());
            if (!build_target(*dep_proj, *dep_target, compiler, cdb)) ok = false;
        }
    }
    return ok;
}

inline bool build_dependency_closure(const Workspace& ws, const WorkspaceProject& proj,
                                      const Target& target, const std::string& compiler, CompileDb& cdb) {
    std::set<std::string> visited;
    return build_dependency_closure_impl(ws, proj, target, compiler, cdb, visited);
}

inline bool build_workspace(const Workspace& ws, const std::string& only = "") {
    const auto& projects = ws.is_workspace
        ? ws.members
        : std::vector<WorkspaceProject>{ws.root_project};

    // Only require a C++ compiler if something actually needs one - a
    // pure-Hylian project has no business failing because clang++/g++
    // aren't installed.
    bool needs_cpp_compiler = false;
    for (auto& proj : projects) {
        if (!only.empty() && proj.file.name != only) continue;
        for (auto& target : proj.file.targets)
            if (!is_hylian_target(target)) needs_cpp_compiler = true;
    }

    std::string compiler;
    if (needs_cpp_compiler) {
        compiler = detect_compiler();
        if (compiler.empty()) {
            log_err("No compiler found (install clang++ or g++)");
            return false;
        }
        log_info("Using compiler: " + compiler);
    }

    bool ok = true;
    CompileDb cdb;

    // pass 1: staticlibs first (dep order) - Hylian doesn't have a
    // staticlib target type yet, so this pass is C++-only.
    for (auto& proj : projects) {
        if (!only.empty() && proj.file.name != only) continue;
        for (auto& target : proj.file.targets) {
            if (target.type != "staticlib") continue;
            if (is_hylian_target(target)) {
                log_err("target '" + target.name + "': type \"staticlib\" isn't supported for Hylian sources yet");
                ok = false;
                continue;
            }
            std::printf("\n%sBuilding%s %s\n", Color::Bold, Color::Reset, proj.file.name.c_str());
            if (!build_target(proj, target, compiler, cdb)) ok = false;
        }
    }

    // pass 2: everything else
    for (auto& proj : projects) {
        if (!only.empty() && proj.file.name != only) continue;
        for (auto& target : proj.file.targets) {
            if (target.type == "staticlib") continue;
            std::printf("\n%sBuilding%s %s\n", Color::Bold, Color::Reset, proj.file.name.c_str());
            bool target_ok = is_hylian_target(target)
                ? build_hylian_target(proj, target)
                : build_target(proj, target, compiler, cdb);
            if (!target_ok) ok = false;
        }
    }

    write_compile_db(cdb);

    return ok;
}

} // namespace Zora
