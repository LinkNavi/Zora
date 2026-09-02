#pragma once
#include "Logger.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <functional>

namespace Zora {

namespace fs = std::filesystem;

// matches a single path component against a glob pattern (*, ?)
inline bool glob_match(std::string_view pattern, std::string_view name) {
    size_t p = 0, n = 0;
    size_t star_p = std::string_view::npos, star_n = 0;

    while (n < name.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n])) {
            ++p; ++n;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star_p = p++;
            star_n = n;
        } else if (star_p != std::string_view::npos) {
            p = star_p + 1;
            n = ++star_n;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

// expand a glob pattern like "src/**/*.cpp" relative to root
inline std::vector<fs::path> glob(const fs::path& root, const std::string& pattern) {
    std::vector<fs::path> results;

    // split pattern on '/'
    std::vector<std::string> parts;
    std::string seg;
    for (char c : pattern) {
        if (c == '/') { if (!seg.empty()) { parts.push_back(seg); seg.clear(); } }
        else seg += c;
    }
    if (!seg.empty()) parts.push_back(seg);

    // recursive walk
    std::function<void(const fs::path&, size_t)> walk = [&](const fs::path& dir, size_t pi) {
        if (pi >= parts.size()) return;

        const auto& part = parts[pi];
        bool is_last = (pi + 1 == parts.size());
        bool is_globstar = (part == "**");

        if (is_globstar) {
            // match rest from current dir (** = zero dirs)
            if (pi + 1 < parts.size())
                walk(dir, pi + 1);

            if (!fs::exists(dir)) return;
            for (auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_directory())
                    walk(entry.path(), pi); // recurse staying on **
            }
            return;
        }

        if (!fs::exists(dir)) return;
        for (auto& entry : fs::directory_iterator(dir)) {
            bool name_match = glob_match(part, entry.path().filename().string());
            if (!name_match) continue;

            if (is_last && entry.is_regular_file()) {
                results.push_back(entry.path());
                log_debug("  glob hit: " + entry.path().string());
            } else if (!is_last && entry.is_directory()) {
                walk(entry.path(), pi + 1);
            }
        }
    };

    walk(root, 0);
    return results;
}

// expand a list of patterns, all relative to root
inline std::vector<fs::path> glob_all(const fs::path& root, const std::vector<std::string>& patterns) {
    std::vector<fs::path> all;
    for (auto& p : patterns)
        for (auto& f : glob(root, p))
            all.push_back(f);
    return all;
}

} // namespace Zora
