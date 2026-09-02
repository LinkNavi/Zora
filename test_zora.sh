#!/usr/bin/env bash

ZORA=${1:-zora}
PASS=0
FAIL=0
ERRORS=()

green="\033[1;32m"
red="\033[1;31m"
bold="\033[1m"
reset="\033[0m"

pass() { echo -e "  ${green}✓${reset} $1"; ((PASS++)); }
fail() { echo -e "  ${red}✗${reset} $1"; ((FAIL++)); ERRORS+=("$1"); }

check() {
    local desc="$1"; shift
    if "$@" > /dev/null 2>&1; then pass "$desc"
    else fail "$desc"; fi
}

TESTROOT=$(mktemp -d)
trap 'echo -e "\nTest dir: $TESTROOT"' EXIT
echo -e "\n${bold}Test root:${reset} $TESTROOT\n"

# ── Test 1: single project build ─────────────────────────────────────────────
echo -e "${bold}[1] Single project build${reset}"
P1="$TESTROOT/single"
mkdir -p "$P1/src" "$P1/include"

cat > "$P1/Zora.toml" << 'TOML'
[project]
name = "single"
version = "0.1.0"

[target.single]
type = "bin"
sources = ["src/**/*.cpp"]
include_dirs = ["include"]
TOML

cat > "$P1/include/Math.hpp" << 'HPP'
#pragma once
int add(int a, int b);
HPP

cat > "$P1/src/Math.cpp" << 'CPP'
#include "Math.hpp"
int add(int a, int b) { return a + b; }
CPP

cat > "$P1/src/main.cpp" << 'CPP'
#include "Math.hpp"
#include <cstdio>
int main() {
    std::printf("add(2,3) = %d\n", add(2, 3));
    return 0;
}
CPP

cd "$P1"
check "build succeeds"        $ZORA build
check "binary exists"         test -f zora-build/bin/single
check "binary runs correctly" bash -c './zora-build/bin/single | grep -q "add(2,3) = 5"'

OUT=$($ZORA build 2>&1)
if echo "$OUT" | grep -q "Up to date"; then pass "incremental: up to date"
else fail "incremental: expected up to date"; fi

touch src/Math.cpp
OUT=$($ZORA build 2>&1)
if echo "$OUT" | grep -q "Compiling"; then pass "incremental: recompiles changed file"
else fail "incremental: did not recompile"; fi

# ── Test 2: clean ─────────────────────────────────────────────────────────────
echo -e "\n${bold}[2] Clean${reset}"
cd "$P1"
check "clean runs"        $ZORA clean
check "build dir removed" test ! -d zora-build

# ── Test 3: staticlib ─────────────────────────────────────────────────────────
echo -e "\n${bold}[3] Static library target${reset}"
P2="$TESTROOT/staticlib"
mkdir -p "$P2/src" "$P2/include"

cat > "$P2/Zora.toml" << 'TOML'
[project]
name = "mylib"
version = "0.1.0"

[target.mylib]
type = "staticlib"
sources = ["src/**/*.cpp"]
include_dirs = ["include"]
TOML

cat > "$P2/include/Greet.hpp" << 'HPP'
#pragma once
#include <string>
std::string greet(const std::string& name);
HPP

cat > "$P2/src/Greet.cpp" << 'CPP'
#include "Greet.hpp"
std::string greet(const std::string& name) { return "Hello, " + name + "!"; }
CPP

cd "$P2"
check "staticlib builds"  $ZORA build
check "libmylib.a exists" test -f zora-build/bin/mylib.a

# ── Test 4: workspace ─────────────────────────────────────────────────────────
echo -e "\n${bold}[4] Workspace build${reset}"
WS="$TESTROOT/workspace"
mkdir -p "$WS/core/src" "$WS/core/include" "$WS/app/src"

cat > "$WS/Zora.toml" << 'TOML'
[workspace]
members = ["core", "app"]
TOML

cat > "$WS/core/Zora.toml" << 'TOML'
[project]
name = "core"
version = "0.1.0"

[target.core]
type = "staticlib"
sources = ["src/**/*.cpp"]
include_dirs = ["include"]
TOML

cat > "$WS/core/include/Core.hpp" << 'HPP'
#pragma once
int multiply(int a, int b);
HPP

cat > "$WS/core/src/Core.cpp" << 'CPP'
#include "Core.hpp"
int multiply(int a, int b) { return a * b; }
CPP

cat > "$WS/app/Zora.toml" << 'TOML'
[project]
name = "app"
version = "0.1.0"

[target.app]
type = "bin"
sources = ["src/**/*.cpp"]
include_dirs = []

[[target.app.deps]]
name = "core"
path = "../core"
TOML

cat > "$WS/app/src/main.cpp" << 'CPP'
#include "Core.hpp"
#include <cstdio>
int main() {
    std::printf("multiply(4,5) = %d\n", multiply(4, 5));
    return 0;
}
CPP

cd "$WS"
check "workspace builds"   $ZORA build
check "core.a exists"      test -f core/zora-build/bin/core.a
check "app binary exists"  test -f app/zora-build/bin/app
check "app runs correctly" bash -c 'app/zora-build/bin/app | grep -q "multiply(4,5) = 20"'
check "build -p core"      $ZORA build -p core

# ── Test 5: run command ───────────────────────────────────────────────────────
echo -e "\n${bold}[5] Run command${reset}"
cd "$P1"
$ZORA build > /dev/null 2>&1 || true
OUT=$($ZORA run single 2>&1)
if echo "$OUT" | grep -q "add(2,3) = 5"; then pass "run executes binary"
else fail "run: wrong output"; fi

# ── Test 6: glob command ──────────────────────────────────────────────────────
echo -e "\n${bold}[6] Glob command${reset}"
cd "$P1"
OUT=$($ZORA glob 2>&1)
if echo "$OUT" | grep -q "main.cpp"; then pass "glob finds main.cpp"
else fail "glob: main.cpp not found"; fi
if echo "$OUT" | grep -q "Math.cpp"; then pass "glob finds Math.cpp"
else fail "glob: Math.cpp not found"; fi

# ── Test 7: header_only = true (forced, no prompt) ───────────────────────────
echo -e "\n${bold}[7] Dep: header_only = true (forced)${reset}"
echo -e "  ${bold}(clones nlohmann/json, skips cmake build)${reset}"

P3="$TESTROOT/header_forced"
mkdir -p "$P3/src"

cat > "$P3/Zora.toml" << 'TOML'
[project]
name = "header_forced"
version = "0.1.0"

[target.header_forced]
type = "bin"
sources = ["src/**/*.cpp"]
include_dirs = []

[[target.header_forced.deps]]
name = "nlohmann_json"
git  = "https://github.com/nlohmann/json"
header_only = true
build_args = []
TOML

cat > "$P3/src/main.cpp" << 'CPP'
#include <nlohmann/json.hpp>
#include <cstdio>
int main() {
    nlohmann::json j;
    j["tool"] = "Zora";
    std::printf("json: %s\n", j.dump().c_str());
    return 0;
}
CPP

cd "$P3"
check "dep clones"        bash -c "$ZORA build > /dev/null 2>&1; test -d zora-deps/nlohmann_json"
check "headers installed" test -d "zora-deps/nlohmann_json/zora-install/include/nlohmann_json"
check "binary built"      test -f zora-build/bin/header_forced
check "binary runs"       bash -c './zora-build/bin/header_forced | grep -q "Zora"'

OUT=$($ZORA build 2>&1)
if echo "$OUT" | grep -q "Cached\|Up to date"; then pass "dep cached on second build"
else fail "dep not cached"; fi

# ── Test 8: header_only = false (forced cmake build) ─────────────────────────
echo -e "\n${bold}[8] Dep: header_only = false (forced cmake build)${reset}"
echo -e "  ${bold}(clones and cmake-builds nlohmann/json — takes a minute)${reset}"

P4="$TESTROOT/cmake_forced"
mkdir -p "$P4/src"

cat > "$P4/Zora.toml" << 'TOML'
[project]
name = "cmake_forced"
version = "0.1.0"

[target.cmake_forced]
type = "bin"
sources = ["src/**/*.cpp"]
include_dirs = []

[[target.cmake_forced.deps]]
name = "nlohmann_json"
git  = "https://github.com/nlohmann/json"
header_only = false
build_args = ["-DJSON_BuildTests=OFF"]
TOML

cat > "$P4/src/main.cpp" << 'CPP'
#include <nlohmann/json.hpp>
#include <cstdio>
int main() {
    nlohmann::json j;
    j["tool"] = "Zora";
    std::printf("json: %s\n", j.dump().c_str());
    return 0;
}
CPP

cd "$P4"
check "cmake dep builds"  $ZORA build
check "headers installed" test -d zora-deps/nlohmann_json/zora-install/include
check "binary built"      test -f zora-build/bin/cmake_forced
check "binary runs"       bash -c './zora-build/bin/cmake_forced | grep -q "Zora"'

# ── Test 9: auto-detect mixed ─────────────────────────────────────────────────
echo -e "\n${bold}[9] Dep: auto-detect mixed — prompts user${reset}"
echo -e "  ${bold}(simulates 'h' keypress then browser enter)${reset}"

P5="$TESTROOT/auto_mixed"
mkdir -p "$P5/src"

cat > "$P5/Zora.toml" << 'TOML'
[project]
name = "auto_mixed"
version = "0.1.0"

[target.auto_mixed]
type = "bin"
sources = ["src/**/*.cpp"]
include_dirs = []

[[target.auto_mixed.deps]]
name = "nlohmann_json"
git  = "https://github.com/nlohmann/json"
build_args = []
TOML

cat > "$P5/src/main.cpp" << 'CPP'
#include <nlohmann/json.hpp>
#include <cstdio>
int main() {
    nlohmann::json j;
    j["tool"] = "Zora";
    std::printf("json: %s\n", j.dump().c_str());
    return 0;
}
CPP

cd "$P5"
rm -rf zora-deps/nlohmann_json/zora-install
OUT=$(printf 'h\n\n' | $ZORA build 2>&1 || true)
if echo "$OUT" | grep -qiE "header|predicted|installed|Choice"; then pass "auto-detect prompted user"
else fail "auto-detect: no prompt shown"; fi
check "binary built after prompt" test -f zora-build/bin/auto_mixed

# ── Test 10: error formatting ─────────────────────────────────────────────────
echo -e "\n${bold}[10] Error formatting${reset}"
P6="$TESTROOT/errors"
mkdir -p "$P6/src"

cat > "$P6/Zora.toml" << 'TOML'
[project]
name = "errors"
version = "0.1.0"

[target.errors]
type = "bin"
sources = ["src/**/*.cpp"]
TOML

cat > "$P6/src/main.cpp" << 'CPP'
int main() {
    undeclared_variable = 5;
    return 0;
}
CPP

cd "$P6"
rm -rf zora-build
OUT=$($ZORA build 2>&1 || true)
if echo "$OUT" | grep -q "error(s)"; then pass "errors reformatted"
else fail "error formatting not applied"; fi
if echo "$OUT" | grep -q "|"; then pass "error pipe lines present"
else fail "error pipe lines missing"; fi

# ── Summary ───────────────────────────────────────────────────────────────────
echo -e "\n${bold}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${reset}"
echo -e "${green}Passed: $PASS${reset}  ${red}Failed: $FAIL${reset}"
if [ ${#ERRORS[@]} -gt 0 ]; then
    echo -e "\n${red}Failed tests:${reset}"
    for e in "${ERRORS[@]}"; do echo -e "  • $e"; done
fi
echo -e "${bold}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${reset}\n"
[ $FAIL -eq 0 ]
