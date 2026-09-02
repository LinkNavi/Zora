#include "Commands.hpp"
#include "Workspace.hpp"
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

void clean(const Zora::Workspace& ws, const std::string& only) {
    const auto& projects = ws.is_workspace
        ? ws.members
        : std::vector<Zora::WorkspaceProject>{ws.root_project};

    for (auto& proj : projects) {
        if (!only.empty() && proj.file.name != only) continue;
        fs::path build_dir = proj.root / "zora-build";
        if (!fs::exists(build_dir)) {
            std::printf("  %s: nothing to clean\n", proj.file.name.c_str());
            continue;
        }
        std::error_code ec;
        fs::remove_all(build_dir, ec);
        if (ec)
            std::printf("  error cleaning %s: %s\n", proj.file.name.c_str(), ec.message().c_str());
        else
            std::printf("  Cleaned %s\n", proj.file.name.c_str());
    }
}
