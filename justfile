build_dir := "build"
bin := build_dir / "zora"

# Default: build release
default: build

# Configure (release)
configure:
    cmake -B {{build_dir}} -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Configure (debug)
configure-debug:
    cmake -B {{build_dir}} -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
build: configure
    cmake --build {{build_dir}} -j$(nproc)
    cp {{build_dir}}/compile_commands.json .

# Build debug
debug: configure-debug
    cmake --build {{build_dir}} -j$(nproc)
    cp {{build_dir}}/compile_commands.json .

# Run
run *ARGS: build
    {{bin}} {{ARGS}}

# Clean
clean:
    rm -rf {{build_dir}}

# Install to /usr/local/bin
install: build
    cmake --install {{build_dir}}

# Cross-compile for arm64
cross-arm64:
    cmake -B build-arm64 -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64
    cmake --build build-arm64 -j$(nproc)

# Cross-compile for riscv64
cross-riscv64:
    cmake -B build-riscv64 -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=riscv64-linux-gnu-g++ \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=riscv64
    cmake --build build-riscv64 -j$(nproc)
