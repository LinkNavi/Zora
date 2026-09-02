#pragma once
#include "Zora.hpp"
#include "ZoraParser.hpp"
#include "Logger.hpp"
#include <filesystem>
#include <vector>
#include <cstdio>

namespace Zora {

namespace fs = std::filesystem;

struct WorkspaceProject {
    fs::path      root;
    ZoraFile      file;
};

struct Workspace {
    bool                          is_workspace;
    WorkspaceProject              root_project;
    std::vector<WorkspaceProject> members;
};

inline Workspace load_workspace(const fs::path& zora_toml) {
    Workspace ws;
    ws.root_project.root = zora_toml.parent_path();
    ws.root_project.file = parse_zora_file(zora_toml);

    log_verbose("Loaded root: " + zora_toml.string());

    if (ws.root_project.file.is_workspace_root()) {
        ws.is_workspace = true;

        if (ws.root_project.file.name.empty()) {
            ws.root_project.file.name = zora_toml.parent_path().filename().string();
            std::printf("Warning: No [project] name in root Zora.toml, using folder name.\n"
                        "  Add: [project]\n"
                        "       name = \"%s\"\n\n", ws.root_project.file.name.c_str());
        }

        std::printf("Workspace: %s\n", ws.root_project.file.name.c_str());

        for (auto& member_path : ws.root_project.file.workspace_members) {
            fs::path member_toml = zora_toml.parent_path() / member_path / "Zora.toml";

            if (!fs::exists(member_toml)) {
                log_err("Member not found: " + member_toml.string());
                continue;
            }

            WorkspaceProject proj;
            proj.root = member_toml.parent_path();
            proj.file = parse_zora_file(member_toml);

            log_verbose("  Loaded member: " + member_path);

            std::printf("  member: %s\n", proj.file.name.c_str());
            for (auto& t : proj.file.targets)
                std::printf("    target: %s [%s]\n", t.name.c_str(), t.type.c_str());

            log_debug("  member path: " + proj.root.string());

            ws.members.push_back(std::move(proj));
        }
    } else {
        ws.is_workspace = false;
        auto& f = ws.root_project.file;
        std::printf("Project: %s %s\n", f.name.c_str(), f.version.c_str());
        for (auto& t : f.targets)
            std::printf("  target: %s [%s]\n", t.name.c_str(), t.type.c_str());

        log_debug("root path: " + ws.root_project.root.string());
    }

    return ws;
}

} // namespace Zora
