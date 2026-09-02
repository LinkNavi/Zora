#pragma once
// Tiny shared helper for running a shell command and capturing its
// stdout+stderr - used by both the C++ builder and the Hylian builder, so
// it lives on its own instead of being duplicated or fought over between
// the two headers.
#include <array>
#include <cstdio>
#include <string>

namespace Zora {

inline int run_capture(const std::string& cmd, std::string& captured_stderr) {
    captured_stderr.clear();
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) return -1;
    std::array<char, 256> buf;
    while (fgets(buf.data(), buf.size(), pipe))
        captured_stderr += buf.data();
    return pclose(pipe);
}

} // namespace Zora
