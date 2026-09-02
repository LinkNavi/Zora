#pragma once
// Interactive search-and-select over the system's pkg-config package
// database — backs `zora dep add --tui`.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace Zora {

struct PkgInfo {
    std::string name;
    std::string desc;
};

// enumerate every package pkg-config knows about on this system
inline std::vector<PkgInfo> list_pkgconfig_packages() {
    std::vector<PkgInfo> out;
    FILE* pipe = popen("pkg-config --list-all 2>/dev/null", "r");
    if (!pipe) return out;

    char buf[512];
    while (fgets(buf, sizeof(buf), pipe)) {
        std::string line(buf);
        size_t i = 0;
        while (i < line.size() && !std::isspace((unsigned char)line[i])) i++;
        std::string name = line.substr(0, i);
        while (i < line.size() && std::isspace((unsigned char)line[i])) i++;
        std::string desc = line.substr(i);
        while (!desc.empty() && (desc.back() == '\n' || desc.back() == '\r')) desc.pop_back();
        if (!name.empty()) out.push_back({name, desc});
    }
    pclose(pipe);

    std::sort(out.begin(), out.end(), [](const PkgInfo& a, const PkgInfo& b) { return a.name < b.name; });
    return out;
}

inline std::string pkg_to_lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// interactive substring search over pkg-config's package list.
// returns the selected package name, or "" if cancelled / nothing found.
inline std::string tui_search_pkgconfig() {
    auto pkgs = list_pkgconfig_packages();
    if (pkgs.empty()) {
        std::puts("no pkg-config packages found on this system");
        return "";
    }

    struct termios old_term, raw_term;
    tcgetattr(STDIN_FILENO, &old_term);
    raw_term = old_term;
    raw_term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);
    std::printf("\033[?25l"); // hide cursor

    std::string search;
    int cursor = 0, scroll = 0;
    const int VISIBLE = 15;
    bool cancelled = false;

    auto filtered = [&]() {
        std::vector<const PkgInfo*> v;
        std::string needle = pkg_to_lower(search);
        for (auto& p : pkgs) {
            if (needle.empty() ||
                pkg_to_lower(p.name).find(needle) != std::string::npos ||
                pkg_to_lower(p.desc).find(needle) != std::string::npos)
                v.push_back(&p);
        }
        return v;
    };

    auto render = [&](const std::vector<const PkgInfo*>& visible) {
        std::printf("\033[2J\033[H");
        std::printf("\033[1mSearch system packages\033[0m  "
                     "\033[2m(type to filter, ↑↓ move, enter select, ctrl-c cancel)\033[0m\n");
        std::printf("  \033[2m/\033[0m %s_\n", search.c_str());
        std::printf("  \033[2m%zu match(es)\033[0m\n", visible.size());
        std::printf("  \033[2m────────────────────────────────────────────\033[0m\n");

        if (cursor >= scroll + VISIBLE) scroll = cursor - VISIBLE + 1;
        if (cursor < scroll) scroll = cursor;

        int shown = 0;
        for (int i = scroll; i < (int)visible.size() && shown < VISIBLE; i++, shown++) {
            bool sel = (i == cursor);
            const char* cursor_str = sel ? "\033[1;36m❯\033[0m" : " ";
            const char* name_col   = sel ? "\033[1;37m" : "\033[0m";
            std::printf("  %s %s%-24s\033[0m \033[2m%s\033[0m\n",
                cursor_str, name_col, visible[i]->name.c_str(), visible[i]->desc.c_str());
        }
        if (visible.empty())
            std::printf("  \033[2m(no matches)\033[0m\n");
        std::fflush(stdout);
    };

    while (true) {
        auto vis = filtered();
        if (vis.empty()) cursor = 0;
        else if (cursor >= (int)vis.size()) cursor = (int)vis.size() - 1;
        render(vis);

        char c = 0;
        if (read(STDIN_FILENO, &c, 1) <= 0) { cancelled = true; break; }

        if (c == '\033') {
            char seq[2];
            read(STDIN_FILENO, &seq[0], 1);
            read(STDIN_FILENO, &seq[1], 1);
            if (seq[0] == '[') {
                if (seq[1] == 'A' && cursor > 0) cursor--;                        // up
                if (seq[1] == 'B' && cursor < (int)vis.size() - 1) cursor++;      // down
            }
        } else if (c == '\n' || c == '\r') {
            break;
        } else if (c == 3) { // ctrl-c
            cancelled = true;
            break;
        } else if (c == 127 || c == 8) { // backspace
            if (!search.empty()) { search.pop_back(); cursor = 0; }
        } else if (c >= 32 && c < 127) {
            search += c;
            cursor = 0;
        }
    }

    std::string result;
    if (!cancelled) {
        auto vis = filtered();
        if (!vis.empty() && cursor < (int)vis.size())
            result = vis[cursor]->name;
    }

    std::printf("\033[?25h\033[2J\033[H"); // show cursor, clear
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    return result;
}

} // namespace Zora
