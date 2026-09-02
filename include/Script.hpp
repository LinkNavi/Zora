#pragma once
#include "Commands.hpp"
#include "Workspace.hpp"
#include "Logger.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int run_script(const Zora::Script& script, const fs::path& proj_root,
                      int extra_argc, char** extra_argv) {
    fs::path script_path = proj_root / script.path;

    if (!fs::exists(script_path)) {
        Zora::log_err("Script not found: " + script_path.string());
        return 1;
    }

    // build command: path + default args + extra args
    std::string cmd = script_path.string();
    for (auto& a : script.default_args) cmd += " " + a;
    for (int i = 0; i < extra_argc; i++) { cmd += " "; cmd += extra_argv[i]; }

    std::printf("\n\033[1mRunning\033[0m %s\n", script.name.c_str());
    Zora::log_debug("$ " + cmd);

    int rc = std::system(cmd.c_str());
    if (rc != 0)
        std::printf("  \033[1;31m✗\033[0m %s exited with code %d\n", script.name.c_str(), rc);
    else
        std::printf("  \033[1;32m✓\033[0m %s\n", script.name.c_str());
    return rc;
}

// find a script by name in a project
static const Zora::Script* find_script(const Zora::ZoraFile& file,
                                        const std::string& name) {
    for (auto& s : file.scripts)
        if (s.name == name) return &s;
    return nullptr;
}

// run all scripts that have `hook` in their before/after list for a project
// returns false if a required script failed
inline bool run_scripts_for_hook(const Zora::WorkspaceProject& proj,
                           const std::string& hook, bool is_before) {
    for (auto& script : proj.file.scripts) {
        auto& hooks = is_before ? script.before : script.after;
        bool matches = false;
        for (auto& h : hooks)
            if (h == hook) { matches = true; break; }
        if (!matches) continue;

        int rc = run_script(script, proj.root, 0, nullptr);
        if (rc != 0 && script.required) {
            std::printf("  \033[1;31merror:\033[0m required script '%s' failed — aborting\n",
                script.name.c_str());
            return false;
        }
    }
    return true;
}

// run scripts for all projects in a workspace for a given hook
inline bool run_hook(const Zora::Workspace& ws, const std::string& hook, bool is_before) {
    const auto& projects = ws.is_workspace
        ? ws.members
        : std::vector<Zora::WorkspaceProject>{ws.root_project};

    for (auto& proj : projects)
        if (!run_scripts_for_hook(proj, hook, is_before))
            return false;
    return true;
}

inline void cmd_script(const Zora::Workspace& ws, int argc, char** argv) {
    if (argc < 1) {
        // list available scripts
        const auto& projects = ws.is_workspace
            ? ws.members
            : std::vector<Zora::WorkspaceProject>{ws.root_project};

        std::puts("Available scripts:");
        for (auto& proj : projects) {
            if (proj.file.scripts.empty()) continue;
            if (ws.is_workspace)
                std::printf("  \033[1m%s\033[0m\n", proj.file.name.c_str());
            for (auto& s : proj.file.scripts) {
                std::printf("    %-20s %s", s.name.c_str(), s.path.c_str());
                if (!s.before.empty()) {
                    std::printf("  [before:");
                    for (auto& b : s.before) std::printf(" %s", b.c_str());
                    std::printf("]");
                }
                if (!s.after.empty()) {
                    std::printf("  [after:");
                    for (auto& a : s.after) std::printf(" %s", a.c_str());
                    std::printf("]");
                }
                if (!s.required) std::printf("  [optional]");
                std::printf("\n");
            }
        }
        return;
    }

    const char* script_name = argv[0];

    // find script across all projects
    const Zora::WorkspaceProject* found_proj = nullptr;
    const Zora::Script*           found_script = nullptr;

    const auto& projects = ws.is_workspace
        ? ws.members
        : std::vector<Zora::WorkspaceProject>{ws.root_project};

    for (auto& proj : projects) {
        auto* s = find_script(proj.file, script_name);
        if (s) { found_proj = &proj; found_script = s; break; }
    }

    if (!found_script) {
        std::printf("error: no script named '%s'\n", script_name);
        return;
    }

    // run any before-scripts that this script depends on
    for (auto& dep : found_script->before) {
        // check if dep is another script
        auto* dep_script = find_script(found_proj->file, dep);
        if (dep_script) {
            int rc = run_script(*dep_script, found_proj->root, 0, nullptr);
            if (rc != 0 && dep_script->required) {
                std::printf("  \033[1;31merror:\033[0m required script '%s' failed\n", dep.c_str());
                return;
            }
        }
    }

    run_script(*found_script, found_proj->root, argc - 1, argv + 1);
}
