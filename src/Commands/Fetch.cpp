#include "Commands.hpp"
#include "Workspace.hpp"
#include "Builder.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static bool run_cmd(const std::string& cmd) {
    Zora::log_debug("$ " + cmd);
    return std::system(cmd.c_str()) == 0;
}

static bool fetch_dep(const Zora::Dep& dep, const fs::path& deps_dir,
                      const std::string& compiler) {
    fs::path dep_dir = deps_dir / dep.name;

    // clone if not present
    if (!fs::exists(dep_dir)) {
        if (dep.git.empty() && dep.path.empty()) {
            std::printf("  error: dep '%s' has no git or path\n", dep.name.c_str());
            return false;
        }

        if (!dep.path.empty()) {
            // local path dep — nothing to fetch
            std::printf("  Local  %s -> %s\n", dep.name.c_str(), dep.path.c_str());
            return true;
        }

        std::printf("  Cloning %s\n", dep.name.c_str());
        if (!run_cmd("git clone --depth=1 " + dep.git + " " + dep_dir.string()))
            return false;
    } else {
        std::printf("  Cached  %s\n", dep.name.c_str());
    }

    // detect build system and invoke it
    fs::path install_dir = dep_dir / "zora-install";
    fs::path stamp       = install_dir / ".built";

    if (fs::exists(stamp)) {
        std::printf("  Built   %s (cached)\n", dep.name.c_str());
        return true;
    }

    fs::create_directories(install_dir);

    // build args string
    std::string extra;
    for (auto& a : dep.build_args) extra += " " + a;

    bool ok = false;
    if (fs::exists(dep_dir / "CMakeLists.txt")) {
        std::printf("  Building %s (cmake)\n", dep.name.c_str());
        fs::path bdir = dep_dir / "zora-cmake-build";
        ok = run_cmd("cmake -B " + bdir.string() +
                     " -DCMAKE_BUILD_TYPE=Release"
                     " -DCMAKE_INSTALL_PREFIX=" + install_dir.string() +
                     " -DCMAKE_CXX_COMPILER=" + compiler +
                     extra + " " + dep_dir.string())
          && run_cmd("cmake --build " + bdir.string() + " -j$(nproc)")
          && run_cmd("cmake --install " + bdir.string());

    } else if (fs::exists(dep_dir / "meson.build")) {
        std::printf("  Building %s (meson)\n", dep.name.c_str());
        fs::path bdir = dep_dir / "zora-meson-build";
        ok = run_cmd("meson setup " + bdir.string() +
                     " --prefix=" + install_dir.string() +
                     " --buildtype=release" +
                     extra + " " + dep_dir.string())
          && run_cmd("ninja -C " + bdir.string())
          && run_cmd("ninja -C " + bdir.string() + " install");
    } else {
        std::printf("  error: dep '%s' has no CMakeLists.txt or meson.build\n", dep.name.c_str());
        return false;
    }

    if (ok) {
        // write stamp
        std::FILE* f = std::fopen(stamp.string().c_str(), "w");
        if (f) std::fclose(f);
        std::printf("  Done    %s\n", dep.name.c_str());
    } else {
        std::printf("  Failed  %s\n", dep.name.c_str());
    }

    return ok;
}

void fetch(const Zora::Workspace& ws, const std::string& only) {
    auto compiler = Zora::detect_compiler();
    if (compiler.empty()) { Zora::log_err("No compiler found"); return; }

    const auto& projects = ws.is_workspace
        ? ws.members
        : std::vector<Zora::WorkspaceProject>{ws.root_project};

    for (auto& proj : projects) {
        if (!only.empty() && proj.file.name != only) continue;

        fs::path deps_dir = proj.root / "zora-deps";
        bool has_deps = false;
        for (auto& t : proj.file.targets)
            if (!t.deps.empty()) { has_deps = true; break; }

        if (!has_deps) {
            std::printf("%s: no deps\n", proj.file.name.c_str());
            continue;
        }

        std::printf("\n\033[1mFetching\033[0m %s\n", proj.file.name.c_str());
        fs::create_directories(deps_dir);

        for (auto& t : proj.file.targets)
            for (auto& dep : t.deps)
                fetch_dep(dep, deps_dir, compiler);
    }
}
