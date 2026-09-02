#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <termios.h>
#include <unistd.h>

namespace Zora {

inline int tui_select(const std::string& prompt, const std::vector<std::string>& options) {
    if (options.empty()) return -1;
    if (options.size() == 1) return 0;

    // save terminal state
    struct termios old_term, raw;
    tcgetattr(STDIN_FILENO, &old_term);
    raw = old_term;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    int selected = 0;
    int n = (int)options.size();

    auto render = [&]() {
        // move up n lines if not first render
        std::printf("\033[%dA", n + 1);
        std::printf("\033[J"); // clear below

        std::printf("\033[1m%s\033[0m\n", prompt.c_str());
        for (int i = 0; i < n; i++) {
            if (i == selected)
                std::printf("  \033[1;36m❯ %s\033[0m\n", options[i].c_str());
            else
                std::printf("    %s\n", options[i].c_str());
        }
        std::fflush(stdout);
    };

    // initial render (no cursor up)
    std::printf("\033[1m%s\033[0m\n", prompt.c_str());
    for (int i = 0; i < n; i++) {
        if (i == selected)
            std::printf("  \033[1;36m❯ %s\033[0m\n", options[i].c_str());
        else
            std::printf("    %s\n", options[i].c_str());
    }
    std::fflush(stdout);

    int result = 0;
    while (true) {
        char c = 0;
        read(STDIN_FILENO, &c, 1);

        if (c == '\033') {
            char seq[2];
            read(STDIN_FILENO, &seq[0], 1);
            read(STDIN_FILENO, &seq[1], 1);
            if (seq[0] == '[') {
                if (seq[1] == 'A') selected = (selected - 1 + n) % n; // up
                if (seq[1] == 'B') selected = (selected + 1) % n;     // down
            }
            render();
        } else if (c == '\n' || c == '\r') {
            result = selected;
            break;
        } else if (c == 'q' || c == 3) { // q or ctrl-c
            result = -1;
            break;
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    std::printf("\n");
    return result;
}

} // namespace Zora
