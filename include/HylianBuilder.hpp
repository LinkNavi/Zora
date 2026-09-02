#pragma once
// HylianBuilder — builds Hylian (.hy) targets.
//
// Deliberately much simpler than the C++ path in Builder.hpp: the `hylian`
// compiler itself walks a program's whole `include { ... }` graph and
// merges it into one object file from a single invocation (see Hylian's
// compiler/compiler.c), so there's no multi-file compile step, no PCH, no
// per-file dependency tracking to reconstruct - just "compile the entry
// file, link the result". No dependency-manager integration either (no
// `deps = [...]` support for Hylian targets yet) - that's an explicit,
// deliberate omission for now, not an oversight.
//
// The one thing a single `hylian` invocation can't produce on its own is
// the small set of runtime pieces the compiler unconditionally requires
// regardless of what the program includes - right now that's the arena
// allocator (`mem.hy`, backing every `new` expression) and the print
// primitives (`runtime.hy`, backing the print/println syntax itself, not
// std.io's print()/println() functions). Those get found in the installed
// stdlib and compiled separately, mirroring how Hylian's own build tool
// (linkle.py) marks certain modules "__always__" linked.
#include "Zora.hpp"
#include "Incremental.hpp"
#include "Glob.hpp"
#include "ErrorFormatter.hpp"
#include "Logger.hpp"
#include "Workspace.hpp"
#include "ProcessRun.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include "DepManager.hpp"
#include "HylianBindgen.hpp"
#include <vector>
#include <sstream>

namespace Zora {

namespace fs = std::filesystem;

inline bool is_hylian_target(const Target& target) {
    for (auto& s : target.sources)
        if (s.size() >= 3 && s.compare(s.size() - 3, 3, ".hy") == 0)
            return true;
    return false;
}

// Checks $PATH first (so e.g. a HYLIAN_LIB-style dev build earlier on
// PATH is honored, same as it would be for any other command), then a
// couple of common install locations as a fallback for setups that don't
// bother putting hylian on PATH.
inline std::string find_hylian_bin() {
    if (const char* explicit_bin = std::getenv("HYLIAN_BIN")) {
        std::error_code ec;
        if (fs::exists(explicit_bin, ec) && !fs::is_directory(explicit_bin, ec))
            return explicit_bin;
    }
    if (const char* path_env = std::getenv("PATH")) {
        std::string paths = path_env;
        size_t start = 0;
        while (start <= paths.size()) {
            size_t colon = paths.find(':', start);
            std::string dir = paths.substr(start, colon == std::string::npos ? colon : colon - start);
            if (!dir.empty()) {
                fs::path candidate = fs::path(dir) / "hylian";
                std::error_code ec;
                if (fs::exists(candidate, ec) && !fs::is_directory(candidate, ec))
                    return candidate.string();
            }
            if (colon == std::string::npos) break;
            start = colon + 1;
        }
    }

    for (const char* candidate : {"/usr/local/bin/hylian", "/usr/bin/hylian"}) {
        std::error_code ec;
        if (fs::exists(candidate, ec) && !fs::is_directory(candidate, ec))
            return candidate;
    }
    return "";
}

inline std::string shell_quote(const std::string& value) {
    std::string out = "'";
    for (char c : value) out += (c == '\'' ? "'\\''" : std::string(1, c));
    return out + "'";
}

// Zora owns this generation step rather than asking users to invoke a tool.
// This keeps .hyi files reproducible build artifacts while leaving the
// compiler and LSP with one source of truth.
inline bool generate_hylian_bindings(const Target& target, const fs::path& root,
                                     const std::string& inc_flags) {
    std::vector<fs::path> include_dirs;
    std::istringstream flags(inc_flags);
    std::string flag;
    while (flags >> flag)
        if (flag.rfind("-I", 0) == 0 && flag.size() > 2)
            include_dirs.emplace_back(flag.substr(2));

    for (const auto& dep : target.deps) {
        if (dep.system.empty()) continue;
        std::vector<fs::path> candidates;
        std::string includedir;
        run_capture("pkg-config --variable=includedir " + shell_quote(dep.system), includedir);
        includedir = trim(includedir);
        if (!includedir.empty()) include_dirs.emplace_back(includedir);
        for (const auto& dir : include_dirs) {
            candidates.push_back(dir / (dep.system + ".h"));
            candidates.push_back(dir / dep.system / (dep.system + ".h"));
        }
        fs::path header;
        for (const auto& candidate : candidates)
            if (fs::exists(candidate)) { header = candidate; break; }
        if (header.empty()) {
            log_debug("no header found for system dependency '" + dep.system + "'; skipping bindgen");
            continue;
        }

        fs::path out = root / "vendors" / dep.system / (dep.system + ".hyi");
        bool current = false;
        if (fs::exists(out) && mtime(out) >= mtime(header)) {
            std::ifstream existing(out);
            std::string first_line;
            std::getline(existing, first_line);
            current = first_line == "// Generated by Zora using libclang v2";
        }
        if (current) continue;
        fs::create_directories(out.parent_path());
        std::printf("  Generating Hylian bindings for %s\n", dep.system.c_str());
        if (!generate_hyi_from_header(header, "vendors." + dep.system, dep.system, out, include_dirs)) {
            log_err("libclang could not parse " + header.string());
            return false;
        }
    }
    return true;
}

// Mirrors linkle.py's _find_runtime_dir() / the compiler's own HYLIAN_LIB
// resolution: env var first (a std/ subdir if present, else the var
// itself), then the well-known system install locations.
inline fs::path find_hylian_stdlib_dir() {
    if (const char* lib = std::getenv("HYLIAN_LIB")) {
        fs::path base = lib;
        std::error_code ec;
        if (fs::is_directory(base / "std", ec)) return base / "std";
        if (fs::is_directory(base, ec)) return base;
    }

    for (const char* candidate : {
             "/usr/local/lib/hylian/std",
             "/usr/lib/hylian/std",
         }) {
        std::error_code ec;
        if (fs::is_directory(candidate, ec)) return candidate;
    }

    if (const char* home = std::getenv("HOME")) {
        fs::path candidate = fs::path(home) / ".hylian" / "std";
        std::error_code ec;
        if (fs::is_directory(candidate, ec)) return candidate;
    }

    return {};
}

// Compiles (or reuses a fresh cached copy of) a stdlib module that must
// always be linked in, regardless of what the program's own include{}
// graph pulls in. Prefers a pre-built .o, then Hylian source (compiled
// with the hylian compiler itself), then C source (for any pieces not
// yet ported off the old C runtime) - same preference order as linkle.py's
// _runtime_obj(). Returns false only on an actual compile failure; a
// module simply not existing in this stdlib isn't an error here; the
// link step is what will complain if it turns out to be needed.
inline bool compile_always_obj(const std::string& hylian_bin, const fs::path& stdlib_dir,
                                const fs::path& cache_dir, const std::string& stem,
                                fs::path& out_obj, std::string& err_out, bool& found) {
    found = false;
    fs::path pre_o = stdlib_dir / (stem + ".o");
    if (fs::exists(pre_o)) {
        found = true;
        out_obj = pre_o;
        return true;
    }

    fs::path hy_src = stdlib_dir / (stem + ".hy");
    if (fs::exists(hy_src)) {
        found = true;
        fs::create_directories(cache_dir);
        out_obj = cache_dir / (stem + ".o");
        if (fs::exists(out_obj) && mtime(out_obj) > mtime(hy_src)) return true;

        std::string cmd = hylian_bin + " " + hy_src.string()
                         + " -o " + out_obj.string()
                         + " --src-dir " + stdlib_dir.string();
        log_debug("cmd: " + cmd);
        return run_capture(cmd, err_out) == 0;
    }

    fs::path c_src = stdlib_dir / (stem + ".c");
    if (fs::exists(c_src)) {
        found = true;
        fs::create_directories(cache_dir);
        out_obj = cache_dir / (stem + ".o");
        if (fs::exists(out_obj) && mtime(out_obj) > mtime(c_src)) return true;

        std::string cmd = "cc -O2 -c " + c_src.string() + " -o " + out_obj.string();
        log_debug("cmd: " + cmd);
        return run_capture(cmd, err_out) == 0;
    }

    return true; // not found anywhere - not a failure, just nothing to add
}

inline bool build_hylian_target(const WorkspaceProject& proj, const Target& target) {
    fs::path build_dir = proj.root / "zora-build" / target.name;
    fs::path bin_dir   = proj.root / "zora-build" / "bin";
    fs::path cache_dir = proj.root / "zora-build" / ".hylian-cache";
    fs::create_directories(build_dir);
    fs::create_directories(bin_dir);

    auto entries = glob_all(proj.root, target.sources);
    if (entries.empty()) {
        log_err("No .hy entry source found for target: " + target.name);
        return false;
    }
    if (entries.size() > 1) {
        log_err("Hylian target '" + target.name + "' lists " + std::to_string(entries.size())
              + " source files, but Hylian compiles from a single entry point and follows "
                "its own include{} graph from there - list just the entry, e.g. "
                "sources = [\"src/main.hy\"]");
        return false;
    }
    fs::path entry = entries[0];

    std::string hylian_bin = find_hylian_bin();
    if (hylian_bin.empty()) {
        log_err("hylian compiler not found (checked PATH, /usr/local/bin, /usr/bin)");
        return false;
    }

    fs::path obj = build_dir / (target.name + ".o");
    fs::path bin = bin_dir / target.name;
    std::string inc_flags, lib_flags;
        if (!resolve_deps(target, proj.root, inc_flags, lib_flags)) {
            log_err("failed to resolve dependencies for Hylian target: " + target.name);
            return false;
        }
    if (!generate_hylian_bindings(target, proj.root, inc_flags)) {
        log_err("failed to generate Hylian FFI bindings");
        return false;
    }
    // Incremental rebuild check. A single hylian invocation follows its
    // own include{} graph internally, so unlike the C++ path there's no
    // -MMD-style per-file dependency list to read back - the closest
    // cheap approximation is "any .hy file anywhere under the project is
    // newer than the last build", which over-triggers slightly (touching
    // an unrelated .hy file rebuilds every target) but never under-builds.
    bool stale = !fs::exists(obj);
    if (!stale) {
        auto obj_time = mtime(obj);
        for (auto& f : glob_all(proj.root, {"**/*.hy"}))
            if (mtime(f) > obj_time) { stale = true; break; }
    }

    // A "kernel" target has no OS underneath it to link against: it compiles
    // with --freestanding --target limine (renames the user's `main` to the
    // `_start` symbol Limine jumps to - see compiler/codegen_hygen.c - and
    // suppresses hosted-environment assumptions) and links with `ld -T`
    // against the Limine runtime support object instead of `gcc`.
    bool is_kernel = (target.type == "kernel");

    if (stale) {
        std::printf("  %sCompiling%s %s\n", Color::Cyan, Color::Reset,
            fs::relative(entry, proj.root).c_str());
        // The compiler resolves vendor packages relative to --src-dir's
        // parent.  Point it at the entry's source directory so a normal
        // project layout (src/ + vendors/) resolves vendors at the project
        // root instead of its parent directory.
        fs::path source_dir = entry.parent_path();
        std::string cmd = hylian_bin + " " + entry.string()
                         + " -o " + obj.string()
                         + " --src-dir " + source_dir.string();
        if (is_kernel) cmd += " --freestanding --target limine";
        log_debug("cmd: " + cmd);
        std::string err;
        int rc = run_capture(cmd, err);
        if (!err.empty()) format_stderr(err);
        if (rc != 0) return false;
    } else {
        std::printf("  %sUp to date%s %s\n", Color::Blue, Color::Reset, target.name.c_str());
    }

    fs::path stdlib_dir = find_hylian_stdlib_dir();
    std::vector<fs::path> extra_objs;
    if (!stdlib_dir.empty()) {
        // "mem" backs `new`/arena allocation, "runtime" backs the
        // print/println syntax (hylian_print/hylian_println) - both are
        // compiler-mandated regardless of what the program itself
        // includes, so both normally always get linked in. NOT for a kernel
        // target, though: both call raw Linux mmap/munmap/write syscalls
        // (see stdlib/mem.hy, stdlib/runtime.hy) that mean nothing with no
        // OS underneath. The Limine runtime object supplies freestanding-
        // safe replacements for the same symbol names instead (see
        // runtime/platform/limine.c's arena_init/arena_alloc/arena_free and
        // hylian_print/hylian_println/hylian_int_to_str) - linking both
        // would be a straight multiple-definition error anyway.
        if (!is_kernel) {
            for (const char* stem : {"mem", "runtime"}) {
                fs::path always_obj;
                std::string err;
                bool found = false;
                if (!compile_always_obj(hylian_bin, stdlib_dir, cache_dir, stem, always_obj, err, found)) {
                    log_err(std::string("failed to build required stdlib module '") + stem + "'");
                    if (!err.empty()) format_stderr(err);
                    return false;
                }
                if (found) extra_objs.push_back(always_obj);
            }
        }
    } else {
        log_debug("no installed Hylian stdlib found "
                  "(checked $HYLIAN_LIB, /usr/local/lib/hylian/std, /usr/lib/hylian/std, ~/.hylian/std)");
    }

    std::string link_cmd;
    if (is_kernel) {
        if (stdlib_dir.empty()) {
            log_err("kernel target '" + target.name + "' needs the installed Hylian stdlib "
                     "to find the Limine runtime support object (checked $HYLIAN_LIB, "
                     "/usr/local/lib/hylian/std, /usr/lib/hylian/std, ~/.hylian/std)");
            return false;
        }
        fs::path platform_dir = stdlib_dir / "platform";
        fs::path limine_o  = platform_dir / "limine.o";
        fs::path limine_ld = platform_dir / "limine.ld";
        if (!fs::exists(limine_o) || !fs::exists(limine_ld)) {
            log_err("Limine runtime support not found at " + platform_dir.string()
                   + " (expected limine.o and limine.ld) - is your Hylian install's "
                     "stdlib missing platform objects? See install.sh's PLATFORM_DIR.");
            return false;
        }

        std::printf("  %sLinking%s   %s\n", Color::White, Color::Reset, target.name.c_str());
        link_cmd = "ld -T " + limine_ld.string() + " " + obj.string();
        for (auto& o : extra_objs) link_cmd += " " + o.string();
        link_cmd += " " + limine_o.string();
        link_cmd += " -o " + bin.string();
     } else {
            std::printf("  %sLinking%s   %s\n", Color::White, Color::Reset, target.name.c_str());
            link_cmd = "gcc " + obj.string();
            for (auto& o : extra_objs) link_cmd += " " + o.string();

            // Pass system .so flags (e.g. -lvulkan -lGL -lX11)
            if (!lib_flags.empty()) link_cmd += " " + lib_flags;
            std::cout << lib_flags << std::endl;

            link_cmd += " -o " + bin.string();
    #ifndef __APPLE__
            link_cmd += " -no-pie";
    #endif
     }
    log_debug("link: " + link_cmd);
    std::string link_err;
    int rc = run_capture(link_cmd, link_err);
    if (!link_err.empty()) format_stderr(link_err);
    if (rc != 0) return false;

    std::printf("  %sFinished%s  %s\n", Color::Bold, Color::Reset, target.name.c_str());
    return true;
}

} // namespace Zora
