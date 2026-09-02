#include "Commands.hpp"
#include "Workspace.hpp"
#include "Builder.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

void run(const Zora::Workspace& ws, const char* target_name, int extra_argc, char** extra_argv) {
    // find the target
    const Zora::WorkspaceProject* found_proj = nullptr;
    const Zora::Target*           found_target = nullptr;

    const auto& projects = ws.is_workspace
        ? ws.members
        : std::vector<Zora::WorkspaceProject>{ws.root_project};

    for (auto& proj : projects) {
        for (auto& t : proj.file.targets) {
            if (t.type != "bin") continue;
            if (!target_name || std::strcmp(t.name.c_str(), target_name) == 0) {
                found_proj   = &proj;
                found_target = &t;
                break;
            }
        }
        if (found_target) break;
    }

    if (!found_target) {
        if (target_name)
            std::printf("error: no binary target named '%s'\n", target_name);
        else
            std::puts("error: no binary targets found");
        return;
    }

    // Rebuild whatever found_target depends on (transitively, through local
    // workspace path deps) before touching found_target itself — otherwise
    // a dirty dependency (e.g. Engine) silently links stale, since
    // resolve_deps() only checks that its .a *exists*, not that it's fresh.
    Zora::CompileDb cdb;
    if (!Zora::is_hylian_target(*found_target)) {
        auto compiler = Zora::detect_compiler();
        if (compiler.empty()) { Zora::log_err("No compiler found"); return; }
        if (!Zora::build_dependency_closure(ws, *found_proj, *found_target, compiler, cdb)) {
            std::puts("error: build failed");
            return;
        }
    }

    // build first
    std::printf("\n\033[1mBuilding\033[0m %s\n", found_proj->file.name.c_str());

    bool built;
    if (Zora::is_hylian_target(*found_target)) {
        built = Zora::build_hylian_target(*found_proj, *found_target);
    } else {
        auto compiler = Zora::detect_compiler();
        if (compiler.empty()) { Zora::log_err("No compiler found"); return; }
        built = Zora::build_target(*found_proj, *found_target, compiler, cdb);
        Zora::write_compile_db(cdb);
    }
    if (!built) {
        std::puts("error: build failed");
        return;
    }

    // construct binary path
    fs::path bin = found_proj->root / "zora-build" / "bin" / found_target->name;
    if (!fs::exists(bin)) {
        std::printf("error: binary not found at %s\n", bin.c_str());
        return;
    }

    // build command with extra args
    std::string cmd = bin.string();
    for (int i = 0; i < extra_argc; i++) {
        cmd += ' ';
        cmd += extra_argv[i];
    }

    std::printf("\n\033[1mRunning\033[0m %s\n\n", bin.c_str());
    int rc = std::system(cmd.c_str());
    if (rc != 0)
        std::printf("\nprocess exited with code %d\n", rc);
}
