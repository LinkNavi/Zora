
cmake_minimum_required(VERSION 3.10)
project(testing)

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/../src/*.c")

add_executable(testing ${SOURCES})

target_include_directories(testing PRIVATE "${CMAKE_SOURCE_DIR}/../include")

# Optional default flags
target_compile_options(testing PRIVATE -Wall -Wextra)
