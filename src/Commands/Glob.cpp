#include "Commands.hpp"
#include "Glob.hpp"
#include "Compiler.hpp"
#include <cstdio>

void printGlob(const Zora::Workspace& ws) {
    // Purely informational (this command is a dry-run over source globs,
    // not a build), so a missing C++ compiler shouldn't stop a pure-Hylian
    // project from listing its sources.
    auto compiler = Zora::detect_compiler();
    if (compiler.empty())
        Zora::log_err("No C++ compiler found (install clang++ or g++) - listing sources anyway");
    else
        std::printf("Compiler: %s\n\n", compiler.c_str());

    const auto& projects = ws.is_workspace
        ? ws.members
        : std::vector<Zora::WorkspaceProject>{ws.root_project};

    for (auto& proj : projects) {
        std::printf("Project: %s\n", proj.file.name.c_str());
        for (auto& target : proj.file.targets) {
            std::printf("  Target: %s [%s]\n", target.name.c_str(), target.type.c_str());
            auto sources = Zora::glob_all(proj.root, target.sources);
            if (sources.empty()) { std::puts("    (no sources found)"); continue; }
            for (auto& src : sources)
                std::printf("    %s\n", src.c_str());
        }
    }
}
