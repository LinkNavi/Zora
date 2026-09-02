#pragma once
#include <string>
#include <vector>

namespace Zora {

struct Dep {
    std::string name;
    std::string git;
    std::string path;
    std::string system; // pkg-config/system library name — e.g. "glfw3". If set,
                         // this dep is resolved from the system (pkg-config, or a
                         // plain -l<name> fallback) instead of being cloned/built.
    std::vector<std::string> build_args;
    int header_only = -1; // -1=auto, 0=force build, 1=force header-only
};

struct Target {
    std::string name;
    std::string type; // "bin", "lib", "staticlib"
    std::vector<std::string> sources;
    std::vector<std::string> include_dirs;
    std::vector<std::string> compiler_flags;
    std::vector<Dep> deps;
};

struct Script {
    std::string name;
    std::string path;
    std::vector<std::string> default_args;
    std::vector<std::string> before;   // run before these commands/scripts
    std::vector<std::string> after;    // run after these commands/scripts
    bool required = true;              // abort if script fails
};

struct ZoraFile {
    std::string name;
    std::string version;
    std::vector<Target> targets;
    std::vector<Script> scripts;
    std::vector<std::string> workspace_members;

    bool is_workspace_root() const { return !workspace_members.empty(); }
};

} // namespace Zora
