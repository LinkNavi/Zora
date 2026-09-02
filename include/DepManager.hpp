#pragma once
#include "Zora.hpp"
#include "Lock.hpp"
#include "Compiler.hpp"
#include "HeaderOnly.hpp"
#include "Logger.hpp"
#include "ProcessRun.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <string>
#include <termios.h>
#include <unistd.h>

namespace Zora {

namespace fs = std::filesystem;

inline int run_sys(const std::string& cmd) {
    log_debug("$ " + cmd);
    return std::system(cmd.c_str());
}

inline std::string expand_git_url(const std::string& input) {
    if (input.find("://") != std::string::npos) return input;
    if (input.find('/') != std::string::npos)
        return "https://github.com/" + input;
    return input;
}

inline std::string dep_name_from_url(const std::string& url) {
    auto pos = url.rfind('/');
    std::string name = (pos == std::string::npos) ? url : url.substr(pos + 1);
    if (name.size() > 4 && name.substr(name.size() - 4) == ".git")
        name = name.substr(0, name.size() - 4);
    return name;
}

inline bool clone_dep(const std::string& url, const fs::path& dest) {
    if (fs::exists(dest)) return true;
    std::printf("  Cloning %s\n", url.c_str());
    return run_sys("git clone --depth=1 " + url + " " + dest.string()) == 0;
}

// prefix_path: semicolon-joined list of sibling deps' install dirs (may be
// empty). Passed through as CMAKE_PREFIX_PATH so a dep's own find_package()
// calls can resolve another Zora-managed dep (e.g. vk-bootstrap finding the
// vulkan-headers dep) instead of silently falling back to whatever (or
// nothing) happens to be installed on the system.
inline bool build_dep(const fs::path& dep_dir, const fs::path& install_dir,
                       const std::string& compiler,
                       const std::vector<std::string>& build_args,
                       const std::string& prefix_path = "") {
    fs::path stamp = install_dir / ".built";
    if (fs::exists(stamp)) return true;

    fs::create_directories(install_dir);
    std::string extra;
    for (auto& a : build_args) extra += " " + a;

    // quoted so the embedded ';' list-separator survives the shell (std::system
    // runs this through /bin/sh -c, where an unquoted ';' ends the command)
    std::string cmake_prefix_flag = prefix_path.empty()
        ? "" : " \"-DCMAKE_PREFIX_PATH=" + prefix_path + "\"";
    std::string prefix_env = prefix_path.empty()
        ? "" : "CMAKE_PREFIX_PATH=\"" + prefix_path + "\" ";

    bool ok = false;
    if (fs::exists(dep_dir / "CMakeLists.txt")) {
        std::printf("  Building (cmake)\n");
        fs::path bdir = dep_dir / "zora-cmake-build";
        // A leftover build dir means a *previous* attempt failed. CMake caches
        // find_package() results in CMakeCache.txt, so reconfiguring in place
        // would keep whatever (wrong) result it cached the first time around
        // — e.g. a retry with a newly-valid CMAKE_PREFIX_PATH would silently
        // do nothing. Wipe it so the retry actually re-resolves everything.
        if (fs::exists(bdir)) fs::remove_all(bdir);
        ok = run_sys(prefix_env + "cmake -B " + bdir.string() +
                     " -DCMAKE_BUILD_TYPE=Release"
                     " -DCMAKE_INSTALL_PREFIX=" + install_dir.string() +
                     " -DCMAKE_CXX_COMPILER=" + compiler +
                     cmake_prefix_flag +
                     extra + " " + dep_dir.string()) == 0
          && run_sys("cmake --build " + bdir.string() + " -j$(nproc)") == 0
          && run_sys("cmake --install " + bdir.string()) == 0;
    } else if (fs::exists(dep_dir / "meson.build")) {
        std::printf("  Building (meson)\n");
        fs::path bdir = dep_dir / "zora-meson-build";
        if (fs::exists(bdir)) fs::remove_all(bdir); // same reasoning as the cmake case above
        ok = run_sys(prefix_env + "meson setup " + bdir.string() +
                     " --prefix=" + install_dir.string() +
                     " --buildtype=release" +
                     extra + " " + dep_dir.string()) == 0
          && run_sys("ninja -C " + bdir.string()) == 0
          && run_sys("ninja -C " + bdir.string() + " install") == 0;
    } else {
        std::printf("  error: no CMakeLists.txt or meson.build\n");
        return false;
    }

    if (ok) {
        std::FILE* f = std::fopen(stamp.string().c_str(), "w");
        if (f) std::fclose(f);
    }
    return ok;
}

inline int detect_install_mode(const fs::path& dep_dir, int toml_flag) {
    if (toml_flag == 1) return 1;
    if (toml_flag == 0) return 0;

    bool has_build = fs::exists(dep_dir / "CMakeLists.txt") ||
                     fs::exists(dep_dir / "meson.build");
    auto predicted = predict_include_dirs(dep_dir);
    bool has_headers = !predicted.empty();

    if (!has_build && has_headers)  return 1;
    if (has_build  && !has_headers) return 0;
    if (!has_build && !has_headers) return 0;
    return -1;
}

inline bool ask_header_only(const std::string& name) {
    std::printf("\n\033[1m%s\033[0m has both a build system and header dirs.\n", name.c_str());
    std::printf("  [b] build via cmake/meson\n");
    std::printf("  [h] install as header-only\n");
    std::printf("  Choice: ");
    std::fflush(stdout);

    struct termios old, raw;
    tcgetattr(STDIN_FILENO, &old);
    raw = old;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    char c = 0;
    read(STDIN_FILENO, &c, 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    std::printf("%c\n", c);

    return c == 'h' || c == 'H';
}

// inject include flags for an installed dep
// adds both zora-install/include AND zora-install/include/<depname>
// so internal relative includes (e.g. <nlohmann/foo.hpp>) resolve correctly
inline void inject_inc_flags(const fs::path& install_dir, const std::string& name,
                              std::string& out_inc_flags) {
    fs::path inc = install_dir / "include";
    if (!fs::exists(inc)) return;
    out_inc_flags += " -I" + inc.string();                    // include/
    fs::path inc_dep = inc / name;
    if (fs::exists(inc_dep)) {
        out_inc_flags += " -I" + inc_dep.string();            // include/nlohmann_json/
        fs::path inc_dep_inner = inc_dep / "include";
        if (fs::exists(inc_dep_inner))
            out_inc_flags += " -I" + inc_dep_inner.string();  // include/nlohmann_json/include/
    }
}

inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// resolve a system dep (no cloning/building) — try pkg-config first, and if
// there's no .pc file for it, fall back to a plain "-l<name>" and hope it's
// on the linker's default search path (fine for things like pthread, dl, m).
inline bool resolve_system_dep(const std::string& pkg,
                                std::string& out_inc_flags, std::string& out_lib_flags) {
    std::printf("\n\033[1mDep\033[0m %s \033[2m(system)\033[0m\n", pkg.c_str());

    if (std::system(("pkg-config --exists " + pkg + " 2>/dev/null").c_str()) == 0) {
        std::string cflags, libs;
        run_capture("pkg-config --cflags " + pkg, cflags);
        run_capture("pkg-config --libs "   + pkg, libs);
        out_inc_flags += " " + trim(cflags);
        out_lib_flags += " " + trim(libs);
        std::printf("  Found via pkg-config\n");
        return true;
    }

    out_lib_flags += " -l" + pkg;
    std::printf("  No pkg-config data for '%s' — linking -l%s directly\n", pkg.c_str(), pkg.c_str());
    return true;
}

inline bool resolve_deps(const Target& target, const fs::path& proj_root,
                          std::string& out_inc_flags, std::string& out_lib_flags) {
    if (target.deps.empty()) return true;

    auto compiler  = detect_compiler();
    fs::path deps_dir  = proj_root / "zora-deps";
    fs::path lock_path = proj_root / "Zora.lock";
    LockFile lf = read_lock(lock_path);

    bool all_ok = true;

    struct GitDep {
        const Dep* src;
        std::string name;
        std::string url;
        fs::path dep_dir;
        fs::path install_dir;
        int  mode     = -2;    // -2=undecided, -1=ask, 0=build, 1=header-only
        bool cloned   = false;
        bool resolved = false;
    };
    std::vector<GitDep> git_deps;

    // local/workspace deps don't need cloning/building — resolve immediately
    for (auto& dep : target.deps) {
        if (dep.path.empty()) continue;
        fs::path dep_root = fs::absolute(proj_root / dep.path);
        fs::path lib = dep_root / "zora-build" / "bin" / (dep.name + ".a");
        fs::path inc = dep_root / "include";
        if (fs::exists(inc)) out_inc_flags += " -I" + inc.string();
        if (fs::exists(lib)) out_lib_flags += " " + lib.string();
        else { log_err("dep lib not found: " + lib.string() + " — build it first"); all_ok = false; }
    }

    // system deps (pkg-config / plain -l<name>) — no cloning/building either
    for (auto& dep : target.deps) {
        if (dep.system.empty()) continue;
        if (!resolve_system_dep(dep.system, out_inc_flags, out_lib_flags)) all_ok = false;
    }

    for (auto& dep : target.deps) {
        if (!dep.path.empty() || !dep.system.empty()) continue;
        std::string url  = expand_git_url(dep.git);
        std::string name = dep.name.empty() ? dep_name_from_url(url) : dep.name;
        fs::path dep_dir     = deps_dir / name;
        git_deps.push_back({&dep, name, url, dep_dir, dep_dir / "zora-install"});
    }

    if (git_deps.empty()) {
        write_lock(lock_path, lf);
        return all_ok;
    }

    fs::create_directories(deps_dir);

    // Let every cmake/meson dep see every sibling dep's install dir via
    // CMAKE_PREFIX_PATH, so e.g. vk-bootstrap's find_package(VulkanHeaders)
    // picks up the vulkan-headers dep Zora manages instead of the system SDK.
    std::string prefix_path;
    for (auto& g : git_deps)
        prefix_path += (prefix_path.empty() ? "" : ";") + g.install_dir.string();

    // clone everything up front so later passes can see all dep directories
    for (auto& g : git_deps) {
        std::printf("\n\033[1mDep\033[0m %s\n", g.name.c_str());
        g.cloned = clone_dep(g.url, g.dep_dir);
        if (!g.cloned) { all_ok = false; continue; }

        if (!find_entry(lf, g.name))
            lf.deps.push_back({g.name, g.url, get_commit(g.dep_dir), false});

        if (fs::exists(g.install_dir / ".built")) {
            std::printf("  Cached  %s\n", g.name.c_str());
            g.resolved = true;
        }
    }

    // Retry loop: Zora.toml has no notion of "dep depends on dep", so a cmake
    // dep that needs a sibling already installed (vk-bootstrap needing
    // vulkan-headers) may fail on its first attempt. Keep retrying whatever
    // hasn't resolved yet until a full pass makes no further progress.
    bool progress = true;
    while (progress) {
        progress = false;
        for (auto& g : git_deps) {
            if (g.resolved || !g.cloned) continue;

            if (g.mode == -2) {
                g.mode = detect_install_mode(g.dep_dir, g.src->header_only);
                if (g.mode == -1) g.mode = ask_header_only(g.name) ? 1 : 0;
            }

            bool ok = (g.mode == 1)
                ? install_header_only(g.dep_dir, g.install_dir, g.name, g.src->header_only != 1)
                : build_dep(g.dep_dir, g.install_dir, compiler, g.src->build_args, prefix_path);

            if (ok) { g.resolved = true; progress = true; }
        }
    }

    for (auto& g : git_deps) {
        if (!g.cloned) continue; // already reported above
        if (!g.resolved) {
            log_err("failed to build dep: " + g.name);
            all_ok = false;
            continue;
        }

        if (auto* entry = find_entry(lf, g.name)) entry->built = true;

        inject_inc_flags(g.install_dir, g.name, out_inc_flags);

        std::error_code ec;
        fs::path lib_dir = g.install_dir / "lib";
        if (fs::exists(lib_dir))
            for (auto& e : fs::directory_iterator(lib_dir, ec))
                if (e.path().extension() == ".a")
                    out_lib_flags += " " + e.path().string();
    }

    write_lock(lock_path, lf);
    return all_ok;
}

} // namespace Zora
