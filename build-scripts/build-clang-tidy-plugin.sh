#!/usr/bin/env bash

set -exo pipefail

BUILD_TYPE=${BUILD_TYPE:-"RelWithDeb"}

NUM_JOBS=${NUM_JOBS:-$(nproc)}
BUILD_PATH=${BUILD_PATH:-"build"}

echo "Using bash version $BASH_VERSION with $NUM_JOBS jobs"
echo "Creating build files at $BUILD_PATH"

# We might need binaries installed via pip, so ensure that our personal bin dir is on the PATH
export PATH=$HOME/.local/bin:$PATH

cmake \
  -B "$BUILD_PATH" \
  -G Ninja \
  -DBACKTRACE=ON \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DLLVM_ROOT=$(llvm-config --prefix) \
  -DLLVM_DIR=$(llvm-config --cmakedir) \
  -DCMAKE_PREFIX_PATH=$(llvm-config --prefix) \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DTILES="$TILES" \
  -DSOUND="$SOUND" \
  -DLIBBACKTRACE="${LIBBACKTRACE:-0}" \
  -DLINKER=mold \
  -DLUA=ON \
  -DCATA_CLANG_TIDY_PLUGIN=ON

ninja -C "$BUILD_PATH" -j"$NUM_JOBS" CataAnalyzerPlugin

# The native build leaves out the protocol engine sources; take their commands from an MCP configuration.
cmake \
  -B "$BUILD_PATH-mcp" \
  -G Ninja \
  -DBACKTRACE=ON \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DLIBBACKTRACE="${LIBBACKTRACE:-0}" \
  -DLUA=ON \
  -DMCP=ON

jq -s '.[0] + .[1] | unique_by(.file)' \
  "$BUILD_PATH/compile_commands.json" "$BUILD_PATH-mcp/compile_commands.json" > compile_commands.json
