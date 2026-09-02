#pragma once
#include <cstdlib>
#include <filesystem>
#include <string>

namespace Zora {

inline std::string detect_compiler() {
    if (std::system("clang++ --version > /dev/null 2>&1") == 0)
        return "clang++";
    if (std::system("g++ --version > /dev/null 2>&1") == 0)
        return "g++";
    return "";
}

} // namespace Zora
