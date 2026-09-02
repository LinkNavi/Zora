#include "Greeter.hpp"
#include <cstdio>

std::string greet(const std::string& name) {
    return "Hello, " + name + "!";
}

void greet_many(const char** names, int count) {
    for (int i = 0; i < count; i++)
        std::printf("Hello, %s!\n", names[i]);
}
