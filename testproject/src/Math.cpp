#include "Math.hpp"

int add(int a, int b) { return a + b; }

int multiply(int a, int b) { return a * b; }

float average(int* arr, int len) {
    int sum = 0;
    for (int i = 0; i < len; i++) sum += arr[i];
    return (float)sum / len;
}
