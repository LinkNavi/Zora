#include "Commands.hpp"
#include "Workspace.hpp"
#include "Lock.hpp"
#include "DepManager.hpp"
#include "TUI.hpp"
#include "PkgSearch.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// pick a target interactively or by name
static const Zora::Target* pick_target(const Zora::Workspace& ws,
                                        Zora::WorkspaceProject const** out_proj,
                                        const char* target_hint) {
    struct Choice { const Zora::WorkspaceProject* proj; const Zora::Target* target; };
    std::vector<Choice>      choices;
    std::vector<std::string> labels;

    // pointers into ws itself (not a copy) so *out_proj stays valid after return
    std::vector<const Zora::WorkspaceProject*> projects;
    if (ws.is_workspace) {
        for (auto& p : ws.members) projects.push_back(&p);
    } else {
        projects.push_back(&ws.root_project);
    }

    for (auto* proj : projects)
        for (auto& t : proj->file.targets) {
            if (target_hint && t.name != target_hint) continue;
            choices.push_back({proj, &t});
            labels.push_back(proj->file.name + " / " + t.name + " [" + t.type + "]");
        }

    if (choices.empty()) {
        std::puts("error: no matching targets found");
        return nullptr;
    }
    if (choices.size() == 1) {
        *out_proj = choices[0].proj;
        return choices[0].target;
    }

    int idx = Zora::tui_select("Select a target:", labels);
    if (idx < 0) { std::puts("cancelled"); return nullptr; }

    *out_proj = choices[idx].proj;
    return choices[idx].target;
}

// append dep to Zora.toml (simple text append — avoids re-serializing toml)
static void append_dep_to_toml(const fs::path& toml_path,
                                const std::string& target_name,
                                const std::string& dep_name,
                                const std::string& git_url) {
    std::ofstream f(toml_path, std::ios::app);
    f << "\n[[target." << target_name << ".deps]]\n";
    f << "name = \"" << dep_name << "\"\n";
    f << "git  = \"" << git_url  << "\"\n";
}

// system deps aren't cloned/built — just a pkg-config (or plain -l) name
static void append_system_dep_to_toml(const fs::path& toml_path,
                                       const std::string& target_name,
                                       const std::string& pkg_name) {
    std::ofstream f(toml_path, std::ios::app);
    f << "\n[[target." << target_name << ".deps]]\n";
    f << "system = \"" << pkg_name << "\"\n";
}

static void remove_dep_from_toml(const fs::path& toml_path,
                                  const std::string& dep_name) {
    std::ifstream in(toml_path);
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    // remove [[target.*.deps]] blocks matching dep_name
    std::string out;
    std::istringstream ss(content);
    std::string line;
    bool skipping = false;

    while (std::getline(ss, line)) {
        if (line.find("[[target.") != std::string::npos &&
            line.find(".deps]]")   != std::string::npos) {
            // peek ahead to see if this block is our dep
            std::string block = line + "\n";
            std::string next_line;
            bool found = false;
            while (std::getline(ss, next_line)) {
                if (next_line.empty()) { block += "\n"; break; }
                block += next_line + "\n";
                if ((next_line.find("name") != std::string::npos ||
                     next_line.find("system") != std::string::npos) &&
                    next_line.find(dep_name) != std::string::npos)
                    found = true;
            }
            if (!found) out += block;
            continue;
        }
        out += line + "\n";
    }

    std::ofstream ofs(toml_path);
    ofs << out;
}

void dep_cmd(const Zora::Workspace& ws, int argc, char** argv) {
    if (argc < 1) {
        std::puts("Usage: zora dep <add|remove|update|list> [args]");
        return;
    }

    const char* sub = argv[0];

    // ── list ──────────────────────────────────────────────────────────────
    if (std::strcmp(sub, "list") == 0) {
        fs::path lock_path = ws.root_project.root / "Zora.lock";
        auto lf = Zora::read_lock(lock_path);

        bool any = !lf.deps.empty();
        if (any) {
            std::puts("Deps:");
            for (auto& e : lf.deps)
                std::printf("  %-20s %s  [%s]\n",
                    e.name.c_str(),
                    e.commit.substr(0, 8).c_str(),
                    e.built ? "built" : "not built");
        }

        // system deps aren't versioned/locked — pull them straight from the
        // already-parsed Zora.toml instead
        const auto& projects = ws.is_workspace
            ? ws.members
            : std::vector<Zora::WorkspaceProject>{ws.root_project};

        std::vector<std::string> system_deps;
        for (auto& proj : projects)
            for (auto& t : proj.file.targets)
                for (auto& d : t.deps)
                    if (!d.system.empty())
                        system_deps.push_back(d.system + "  (target: " + t.name + ")");

        if (!system_deps.empty()) {
            any = true;
            std::puts("System deps:");
            for (auto& s : system_deps)
                std::printf("  %s\n", s.c_str());
        }

        if (!any) std::puts("No deps.");
        return;
    }

    // ── add ───────────────────────────────────────────────────────────────
    if (std::strcmp(sub, "add") == 0) {
        auto print_add_usage = []() {
            std::puts("Usage: zora dep add <user/repo> [-t <target>]");
            std::puts("       zora dep add -s <pkg-config-name> [-t <target>]");
            std::puts("       zora dep add --tui [-t <target>]   (search system packages)");
        };

        if (argc < 2) { print_add_usage(); return; }

        bool is_system = false;
        bool use_tui   = false;
        std::string input;
        const char* target_hint = nullptr;

        for (int i = 1; i < argc; i++) {
            if (std::strcmp(argv[i], "--tui") == 0) {
                use_tui = true;
            } else if ((std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--system") == 0) && i + 1 < argc) {
                is_system = true;
                input = argv[++i];
            } else if (std::strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
                target_hint = argv[++i];
            } else if (input.empty()) {
                input = argv[i];
            }
        }

        if (use_tui) {
            input = Zora::tui_search_pkgconfig();
            if (input.empty()) { std::puts("cancelled"); return; }
            is_system = true;
            std::printf("Selected: %s\n", input.c_str());
        }

        if (input.empty()) { print_add_usage(); return; }

        const Zora::WorkspaceProject* proj = nullptr;
        const Zora::Target* target = pick_target(ws, &proj, target_hint);
        if (!target) return;

        fs::path toml_path = proj->root / "Zora.toml";

        // ── system dep: just record it — no cloning, no building, no lockfile
        if (is_system) {
            append_system_dep_to_toml(toml_path, target->name, input);
            std::printf("Added system dep '%s' to target '%s'\n", input.c_str(), target->name.c_str());
            std::puts("Run 'zora build' to resolve it via pkg-config.");
            return;
        }

        std::string git_url = Zora::expand_git_url(input);
        std::string name    = Zora::dep_name_from_url(git_url);

        append_dep_to_toml(toml_path, target->name, name, git_url);

        // clone immediately
        fs::path dep_dir = proj->root / "zora-deps" / name;
        fs::create_directories(proj->root / "zora-deps");
        if (!Zora::clone_dep(git_url, dep_dir)) return;

        // update lockfile
        fs::path lock_path = proj->root / "Zora.lock";
        auto lf = Zora::read_lock(lock_path);
        if (!Zora::find_entry(lf, name))
            lf.deps.push_back({name, git_url, Zora::get_commit(dep_dir), false});
        Zora::write_lock(lock_path, lf);

        std::printf("Added dep '%s' to target '%s'\n", name.c_str(), target->name.c_str());
        std::puts("Run 'zora build' to fetch and build it.");
        return;
    }

    // ── remove ────────────────────────────────────────────────────────────
    if (std::strcmp(sub, "remove") == 0) {
        if (argc < 2) { std::puts("Usage: zora dep remove <name>"); return; }
        std::string name = argv[1];

        const Zora::WorkspaceProject* proj = nullptr;
        const Zora::Target* target = pick_target(ws, &proj, nullptr);
        if (!target) return;

        remove_dep_from_toml(proj->root / "Zora.toml", name);

        // remove from lockfile
        fs::path lock_path = proj->root / "Zora.lock";
        auto lf = Zora::read_lock(lock_path);
        lf.deps.erase(std::remove_if(lf.deps.begin(), lf.deps.end(),
            [&](auto& e){ return e.name == name; }), lf.deps.end());
        Zora::write_lock(lock_path, lf);

        // remove cloned dir
        fs::path dep_dir = proj->root / "zora-deps" / name;
        if (fs::exists(dep_dir)) {
            fs::remove_all(dep_dir);
            std::printf("Removed dep '%s'\n", name.c_str());
        } else {
            std::printf("Dep '%s' not found on disk (removed from toml/lock)\n", name.c_str());
        }
        return;
    }

    // ── update ────────────────────────────────────────────────────────────
    if (std::strcmp(sub, "update") == 0) {
        const char* only = (argc >= 2) ? argv[1] : nullptr;

        const auto& projects = ws.is_workspace
            ? ws.members
            : std::vector<Zora::WorkspaceProject>{ws.root_project};

        for (auto& proj : projects) {
            fs::path lock_path = proj.root / "Zora.lock";
            auto lf = Zora::read_lock(lock_path);
            bool changed = false;

            for (auto& e : lf.deps) {
                if (only && e.name != only) continue;
                fs::path dep_dir = proj.root / "zora-deps" / e.name;
                if (!fs::exists(dep_dir)) continue;

                std::printf("Updating %s... ", e.name.c_str());
                std::fflush(stdout);
                int rc = std::system(("git -C " + dep_dir.string() + " pull --ff-only").c_str());
                if (rc != 0) { std::puts("failed"); continue; }

                std::string new_commit = Zora::get_commit(dep_dir);
                if (new_commit != e.commit) {
                    std::printf("updated %s -> %s\n",
                        e.commit.substr(0,8).c_str(), new_commit.substr(0,8).c_str());
                    e.commit = new_commit;
                    e.built  = false; // invalidate
                    // remove build stamp
                    fs::remove(dep_dir / "zora-install" / ".built");
                    changed = true;
                } else {
                    std::puts("already up to date");
                }
            }
            if (changed) Zora::write_lock(lock_path, lf);
        }
        return;
    }

    std::printf("error: unknown dep subcommand '%s'\n", sub);
}
