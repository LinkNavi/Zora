#include "Math.hpp"
#include "Greeter.hpp"
#include <cstdio>

int main() {
    // math
    std::printf("add(3, 4)      = %d\n", add(3, 4));
    std::printf("multiply(3, 4) = %d\n", multiply(3, 4));
    int arr[] = {1, 2, 3, 4, 5};
    std::printf("average        = %.2f\n", average(arr, 5));

    // greeter
    std::printf("%s\n", greet("Kirby").c_str());
    const char* names[] = {"Link", "Zelda", "Ganon"};
    greet_many(names, 3);

    return 0;
}
