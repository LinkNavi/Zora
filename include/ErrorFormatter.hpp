#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <fstream>

namespace Zora {

// ANSI colors
namespace Color {
    constexpr const char* Reset   = "\033[0m";
    constexpr const char* Bold    = "\033[1m";
    constexpr const char* Red     = "\033[1;31m";
    constexpr const char* Yellow  = "\033[1;33m";
    constexpr const char* Cyan    = "\033[1;36m";
    constexpr const char* White   = "\033[1;37m";
    constexpr const char* Blue    = "\033[1;34m";
}

struct DiagLine {
    std::string file;
    std::string line;
    std::string col;
    std::string kind;   // error, warning, note
    std::string msg;
};

inline bool parse_diag_line(const std::string& raw, DiagLine& out) {
    // format: file:line:col: kind: message
    size_t p0 = raw.find(':');
    if (p0 == std::string::npos) return false;

    // on linux paths don't start with a drive letter so first : is line number
    size_t p1 = raw.find(':', p0 + 1);
    if (p1 == std::string::npos) return false;

    size_t p2 = raw.find(':', p1 + 1);
    if (p2 == std::string::npos) return false;

    size_t p3 = raw.find(':', p2 + 1);
    if (p3 == std::string::npos) return false;

    out.file = raw.substr(0, p0);
    out.line = raw.substr(p0 + 1, p1 - p0 - 1);
    out.col  = raw.substr(p1 + 1, p2 - p1 - 1);

    std::string kind_raw = raw.substr(p2 + 1, p3 - p2 - 1);
    // trim leading space
    size_t ks = kind_raw.find_first_not_of(' ');
    out.kind = (ks == std::string::npos) ? kind_raw : kind_raw.substr(ks);
    out.msg  = raw.substr(p3 + 2); // skip ": "

    return out.kind == "error" || out.kind == "warning" || out.kind == "note";
}

inline void print_diag(const DiagLine& d) {
    const char* color = Color::White;
    if (d.kind == "error")   color = Color::Red;
    if (d.kind == "warning") color = Color::Yellow;
    if (d.kind == "note")    color = Color::Cyan;

    // "error[file:line:col]"
    std::printf("%s%s%s[%s%s:%s:%s%s]\n",
        color, d.kind.c_str(), Color::Reset,
        Color::White, d.file.c_str(), d.line.c_str(), d.col.c_str(), Color::Reset);

    // " | message"
    std::printf("  %s|%s %s\n", Color::Blue, Color::Reset, d.msg.c_str());
    std::printf("  %s|%s\n", Color::Blue, Color::Reset);
}

// Prints a TOML (or any file/line/col-addressable) parse error in the same
// "error[file:line:col]" style as compiler diagnostics, plus a source
// snippet with a caret under the offending column when the file is still
// readable - toml::parse_error only gives us a line/column pair and a
// description, no ready-made snippet like a compiler does.
inline void print_toml_error(const std::string& file, int64_t line, int64_t col,
                              const std::string& msg) {
    DiagLine d;
    d.file = file;
    d.line = std::to_string(line);
    d.col  = std::to_string(col);
    d.kind = "error";
    d.msg  = msg;
    print_diag(d);

    if (line <= 0) return;
    std::ifstream f(file);
    if (!f) return;

    std::string src_line;
    for (int64_t i = 0; i < line; i++)
        if (!std::getline(f, src_line)) return;

    std::printf("  %s|%s %s\n", Color::Blue, Color::Reset, src_line.c_str());
    std::string caret(col > 1 ? static_cast<size_t>(col - 1) : 0, ' ');
    std::printf("  %s|%s %s%s^%s\n", Color::Blue, Color::Reset,
        caret.c_str(), Color::Red, Color::Reset);
}

inline void format_stderr(const std::string& raw_stderr) {
    std::string line;
    int error_count   = 0;
    int warning_count = 0;

    for (size_t i = 0; i <= raw_stderr.size(); i++) {
        if (i == raw_stderr.size() || raw_stderr[i] == '\n') {
            if (!line.empty()) {
                DiagLine d;
                if (parse_diag_line(line, d)) {
                    print_diag(d);
                    if (d.kind == "error")   error_count++;
                    if (d.kind == "warning") warning_count++;
                } else {
                    // pass through lines we can't parse (continuation lines, caret etc.)
                    std::printf("  %s\n", line.c_str());
                }
                line.clear();
            }
        } else {
            line += raw_stderr[i];
        }
    }

    if (error_count || warning_count) {
        std::printf("\n%s%d error(s)%s, %s%d warning(s)%s\n",
            Color::Red,    error_count,   Color::Reset,
            Color::Yellow, warning_count, Color::Reset);
    }
}

} // namespace Zora
