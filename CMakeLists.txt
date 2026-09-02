cmake_minimum_required(VERSION 3.20)
project(zora VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefer clang
find_program(CLANG clang++)
if(CLANG AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Found clang++, preferring over g++")
endif()

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined")

file(GLOB_RECURSE SOURCES src/*.cpp src/**/*.cpp)

add_executable(zora ${SOURCES})
target_include_directories(zora PRIVATE include)
find_library(LIBCLANG clang)
if(NOT LIBCLANG)
    message(FATAL_ERROR "libclang is required to generate Hylian .hyi bindings")
endif()
target_link_libraries(zora PRIVATE pthread ${LIBCLANG})
target_compile_options(zora PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-march=native>
)

install(TARGETS zora DESTINATION bin)
