#pragma once
#include "toml.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace Zora {

namespace fs = std::filesystem;

struct LockEntry {
    std::string name;
    std::string git;
    std::string commit;
    bool        built = false;
};

struct LockFile {
    std::vector<LockEntry> deps;
};

inline LockFile read_lock(const fs::path& path) {
    LockFile lf;
    if (!fs::exists(path)) return lf;

    auto tbl = toml::parse_file(path.string());
    if (auto arr = tbl["dep"].as_array()) {
        for (auto& dval : *arr) {
            auto* d = dval.as_table();
            if (!d) continue;
            LockEntry e;
            if (auto v = d->get_as<std::string>("name"))   e.name   = v->get();
            if (auto v = d->get_as<std::string>("git"))    e.git    = v->get();
            if (auto v = d->get_as<std::string>("commit")) e.commit = v->get();
            if (auto v = d->get_as<bool>("built"))         e.built  = v->get();
            lf.deps.push_back(std::move(e));
        }
    }
    return lf;
}

inline void write_lock(const fs::path& path, const LockFile& lf) {
    std::ofstream f(path);
    for (auto& e : lf.deps) {
        f << "[[dep]]\n";
        f << "name   = \"" << e.name   << "\"\n";
        f << "git    = \"" << e.git    << "\"\n";
        f << "commit = \"" << e.commit << "\"\n";
        f << "built  = "   << (e.built ? "true" : "false") << "\n\n";
    }
}

inline std::string get_commit(const fs::path& repo_dir) {
    std::string out;
    std::string cmd = "git -C " + repo_dir.string() + " rev-parse HEAD 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buf[64];
    if (fgets(buf, sizeof(buf), pipe)) out = buf;
    pclose(pipe);
    // trim newline
    if (!out.empty() && out.back() == '\n') out.pop_back();
    return out;
}

inline LockEntry* find_entry(LockFile& lf, const std::string& name) {
    for (auto& e : lf.deps)
        if (e.name == name) return &e;
    return nullptr;
}

} // namespace Zora
