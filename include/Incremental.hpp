#pragma once
#include "Logger.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace Zora {

namespace fs = std::filesystem;

using TimePoint = fs::file_time_type;

inline TimePoint mtime(const fs::path& p) {
    std::error_code ec;
    auto t = fs::last_write_time(p, ec);
    return ec ? TimePoint::min() : t;
}

// parse a .d file (makefile dep format) into a list of paths
inline std::vector<fs::path> parse_depfile(const fs::path& dep) {
    std::vector<fs::path> deps;
    std::ifstream f(dep);
    if (!f) return deps;

    std::string line, accum;
    while (std::getline(f, line)) {
        // strip trailing backslash
        if (!line.empty() && line.back() == '\\')
            line.pop_back();
        accum += ' ' + line;
    }

    // skip "output: " target part
    auto colon = accum.find(':');
    if (colon == std::string::npos) return deps;
    accum = accum.substr(colon + 1);

    // split on whitespace
    std::istringstream ss(accum);
    std::string token;
    while (ss >> token)
        deps.emplace_back(token);

    return deps;
}

// returns true if src needs recompilation
inline bool needs_rebuild(const fs::path& src, const fs::path& obj, const fs::path& dep) {
    // no .o file yet
    if (!fs::exists(obj)) {
        log_debug("  rebuild (no obj): " + src.string());
        return true;
    }

    TimePoint obj_time = mtime(obj);

    // src newer than obj
    if (mtime(src) > obj_time) {
        log_debug("  rebuild (src changed): " + src.string());
        return true;
    }

    // any dep header newer than obj
    if (fs::exists(dep)) {
        for (auto& d : parse_depfile(dep)) {
            if (fs::exists(d) && mtime(d) > obj_time) {
                log_debug("  rebuild (header changed): " + d.string());
                return true;
            }
        }
    }

    log_debug("  up to date: " + src.string());
    return false;
}

} // namespace Zora
