#!/bin/bash

# Zora Build Tool - Comprehensive Test Script
# This script tests all major features of Zora

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Helper functions
print_header() {
    echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
}

print_test() {
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    echo -e "${BLUE}[TEST $TESTS_TOTAL]${NC} $1"
}

pass_test() {
    TESTS_PASSED=$((TESTS_PASSED + 1))
    echo -e "${GREEN}✓ PASSED${NC} $1\n"
}

fail_test() {
    TESTS_FAILED=$((TESTS_FAILED + 1))
    echo -e "${RED}✗ FAILED${NC} $1\n"
}

cleanup() {
    cd "$ORIGINAL_DIR"
    if [ -d "$TEST_DIR" ]; then
        rm -rf "$TEST_DIR"
    fi
}

# Setup
ORIGINAL_DIR=$(pwd)
TEST_DIR="/tmp/zora_test_$$"
ZORA_BIN="$ORIGINAL_DIR/target/release/Zora"

# Check if Zora is built
if [ ! -f "$ZORA_BIN" ]; then
    echo -e "${YELLOW}Building Zora first...${NC}"
    cargo build --release
fi

# Trap to cleanup on exit
trap cleanup EXIT

print_header "ZORA BUILD TOOL - TEST SUITE"

echo -e "Test Directory: ${YELLOW}$TEST_DIR${NC}"
echo -e "Zora Binary: ${YELLOW}$ZORA_BIN${NC}\n"

mkdir -p "$TEST_DIR"

# ============================================================================
# TEST 1: Basic C Executable Project
# ============================================================================
print_header "TEST 1: Basic C Executable Project"

cd "$TEST_DIR"
mkdir -p test1_c_exec
cd test1_c_exec

print_test "Initialize C executable project"
if $ZORA_BIN init --name hello_c; then
    pass_test "Project initialized"
else
    fail_test "Failed to initialize project"
fi

print_test "Check project structure"
if [ -f "project.toml" ] && [ -f "src/main.c" ] && [ -d "include" ]; then
    pass_test "Project structure created"
else
    fail_test "Missing project files"
fi

print_test "Build debug project"
if $ZORA_BIN build; then
    pass_test "Debug build successful"
else
    fail_test "Debug build failed"
fi

print_test "Check debug executable"
if [ -f "target/debug/hello_c" ] || [ -f "target/debug/hello_c.exe" ]; then
    pass_test "Debug executable created"
else
    fail_test "Debug executable not found"
fi

print_test "Build release project"
if $ZORA_BIN build --release; then
    pass_test "Release build successful"
else
    fail_test "Release build failed"
fi

print_test "Run executable"
if $ZORA_BIN run 2>&1 | grep -q "Hello from hello_c"; then
    pass_test "Executable runs correctly"
else
    fail_test "Executable output incorrect"
fi

print_test "Show project info"
if $ZORA_BIN info; then
    pass_test "Project info displayed"
else
    fail_test "Failed to show project info"
fi

print_test "Clean build artifacts"
if $ZORA_BIN clean; then
    pass_test "Clean successful"
else
    fail_test "Clean failed"
fi

# ============================================================================
# TEST 2: C++ Executable Project
# ============================================================================
print_header "TEST 2: C++ Executable Project"

cd "$TEST_DIR"
mkdir -p test2_cpp_exec
cd test2_cpp_exec

print_test "Initialize C++ executable project"
if $ZORA_BIN init --name hello_cpp --cpp; then
    pass_test "C++ project initialized"
else
    fail_test "Failed to initialize C++ project"
fi

print_test "Check C++ source file"
if [ -f "src/main.cpp" ]; then
    pass_test "C++ source file created"
else
    fail_test "C++ source file not found"
fi

print_test "Build C++ project"
if $ZORA_BIN build; then
    pass_test "C++ build successful"
else
    fail_test "C++ build failed"
fi

print_test "Run C++ executable"
if $ZORA_BIN run 2>&1 | grep -q "Hello from hello_cpp"; then
    pass_test "C++ executable runs correctly"
else
    fail_test "C++ executable output incorrect"
fi

# ============================================================================
# TEST 3: C Library Project
# ============================================================================
print_header "TEST 3: C Library Project"

cd "$TEST_DIR"
mkdir -p test3_c_lib
cd test3_c_lib

print_test "Initialize C library project"
if $ZORA_BIN init --name mylib --lib; then
    pass_test "Library project initialized"
else
    fail_test "Failed to initialize library project"
fi

print_test "Check library files"
if [ -f "src/mylib.c" ] && [ -f "include/mylib.h" ]; then
    pass_test "Library source and header created"
else
    fail_test "Library files not found"
fi

print_test "Build library"
if $ZORA_BIN build; then
    pass_test "Library build successful"
else
    fail_test "Library build failed"
fi

print_test "Check for library artifact"
if ls target/debug/*.a 2>/dev/null || ls target/debug/*.so 2>/dev/null || ls target/debug/*.dylib 2>/dev/null; then
    pass_test "Library artifact created"
else
    fail_test "Library artifact not found"
fi

# ============================================================================
# TEST 4: File Generation
# ============================================================================
print_header "TEST 4: File Generation"

cd "$TEST_DIR"
mkdir -p test4_file_gen
cd test4_file_gen

$ZORA_BIN init --name filetest > /dev/null 2>&1

print_test "Generate new source file"
if $ZORA_BIN new source utils; then
    if [ -f "src/utils.c" ]; then
        pass_test "Source file generated"
    else
        fail_test "Source file not created"
    fi
else
    fail_test "Failed to generate source file"
fi

print_test "Generate new header file"
if $ZORA_BIN new header utils; then
    if [ -f "include/utils.h" ]; then
        pass_test "Header file generated"
    else
        fail_test "Header file not created"
    fi
else
    fail_test "Failed to generate header file"
fi

print_test "Generate new test file"
if $ZORA_BIN new test utils; then
    if [ -f "tests/test_utils.c" ]; then
        pass_test "Test file generated"
    else
        fail_test "Test file not created"
    fi
else
    fail_test "Failed to generate test file"
fi

# ============================================================================
# TEST 5: Check and Syntax Validation
# ============================================================================
print_header "TEST 5: Check and Syntax Validation"

cd "$TEST_DIR"
mkdir -p test5_check
cd test5_check

$ZORA_BIN init --name checktest > /dev/null 2>&1

print_test "Check valid project"
if $ZORA_BIN check; then
    pass_test "Check passed for valid project"
else
    fail_test "Check failed for valid project"
fi

print_test "Check invalid syntax"
echo "int main() { return 0" > src/bad.c  # Missing closing brace
if $ZORA_BIN check 2>&1 | grep -qi "error\|failed"; then
    pass_test "Check correctly detected syntax error"
else
    fail_test "Check did not detect syntax error"
fi

# ============================================================================
# TEST 6: Clean and Cache Management
# ============================================================================
print_header "TEST 6: Clean and Cache Management"

cd "$TEST_DIR"
mkdir -p test6_clean
cd test6_clean

$ZORA_BIN init --name cleantest > /dev/null 2>&1
$ZORA_BIN build > /dev/null 2>&1

print_test "Check cache stats"
if $ZORA_BIN cache stats; then
    pass_test "Cache stats displayed"
else
    fail_test "Failed to show cache stats"
fi

print_test "Clean build artifacts"
if $ZORA_BIN clean; then
    if [ ! -d "target" ] && [ ! -d ".build" ]; then
        pass_test "Build artifacts cleaned"
    else
        fail_test "Build artifacts still exist"
    fi
else
    fail_test "Clean command failed"
fi

print_test "Clear cache"
$ZORA_BIN build > /dev/null 2>&1  # Rebuild first
if $ZORA_BIN cache clear; then
    pass_test "Cache cleared"
else
    fail_test "Failed to clear cache"
fi

# ============================================================================
# TEST 7: Configuration Parsing
# ============================================================================
print_header "TEST 7: Configuration Parsing"

cd "$TEST_DIR"
mkdir -p test7_config
cd test7_config

$ZORA_BIN init --name configtest > /dev/null 2>&1

print_test "Add custom build flags to project.toml"
cat >> project.toml << 'EOF'

[build]
flags = ["-O3", "-march=native"]
defines = { DEBUG = "1", VERSION = "\"1.0.0\"" }
EOF

if $ZORA_BIN build; then
    pass_test "Build with custom flags successful"
else
    fail_test "Build with custom flags failed"
fi

print_test "Verify info command reads config"
if $ZORA_BIN info | grep -q "Build Flags"; then
    pass_test "Info command displays build configuration"
else
    fail_test "Info command does not show build configuration"
fi

# ============================================================================
# TEST 8: Multiple Source Files
# ============================================================================
print_header "TEST 8: Multiple Source Files"

cd "$TEST_DIR"
mkdir -p test8_multi_src
cd test8_multi_src

$ZORA_BIN init --name multifile > /dev/null 2>&1

print_test "Create additional source files"
cat > src/helper.c << 'EOF'
#include <stdio.h>

void helper_function(void) {
    printf("Helper function called\n");
}
EOF

cat > include/helper.h << 'EOF'
#ifndef HELPER_H
#define HELPER_H

void helper_function(void);

#endif
EOF

# Update main.c to use helper
cat > src/main.c << 'EOF'
#include <stdio.h>
#include "helper.h"

int main(void) {
    printf("Hello from multifile!\n");
    helper_function();
    return 0;
}
EOF

print_test "Build project with multiple sources"
if $ZORA_BIN build; then
    pass_test "Multi-file build successful"
else
    fail_test "Multi-file build failed"
fi

print_test "Run multi-file executable"
if $ZORA_BIN run 2>&1 | grep -q "Helper function called"; then
    pass_test "Multi-file executable works correctly"
else
    fail_test "Multi-file executable output incorrect"
fi

# ============================================================================
# TEST 9: Parallel Build Options
# ============================================================================
print_header "TEST 9: Parallel Build Options"

cd "$TEST_DIR"
mkdir -p test9_parallel
cd test9_parallel

$ZORA_BIN init --name paralleltest > /dev/null 2>&1

print_test "Build with specific job count"
if $ZORA_BIN build --jobs 2; then
    pass_test "Build with --jobs flag successful"
else
    fail_test "Build with --jobs flag failed"
fi

print_test "Verbose build"
if $ZORA_BIN build --verbose 2>&1 | grep -q "cmake\|Compiling\|Linking"; then
    pass_test "Verbose output working"
else
    fail_test "Verbose output not showing details"
fi

# ============================================================================
# TEST 10: Error Handling
# ============================================================================
print_header "TEST 10: Error Handling"

cd "$TEST_DIR"

print_test "Init in non-empty directory with existing project.toml"
mkdir -p test10_error
cd test10_error
$ZORA_BIN init > /dev/null 2>&1
if $ZORA_BIN init 2>&1 | grep -qi "already exists"; then
    pass_test "Correctly prevents overwriting existing project"
else
    fail_test "Did not prevent overwriting"
fi

print_test "Build without project.toml"
cd "$TEST_DIR"
mkdir -p test10_no_toml
cd test10_no_toml
if $ZORA_BIN build 2>&1 | grep -qi "not found\|run.*init"; then
    pass_test "Correctly handles missing project.toml"
else
    fail_test "Did not handle missing project.toml"
fi

# ============================================================================
# SUMMARY
# ============================================================================
print_header "TEST SUMMARY"

echo -e "Total Tests: ${CYAN}$TESTS_TOTAL${NC}"
echo -e "Passed:      ${GREEN}$TESTS_PASSED${NC}"
echo -e "Failed:      ${RED}$TESTS_FAILED${NC}"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}  ALL TESTS PASSED! ✓${NC}"
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
    exit 0
else
    echo -e "\n${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${RED}  SOME TESTS FAILED! ✗${NC}"
    echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
    exit 1
fi