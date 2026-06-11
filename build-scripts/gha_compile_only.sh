#!/bin/bash

# Script made specifically for compiling without running tests on GitHub Actions

echo "Using bash version $BASH_VERSION"
set -exo pipefail

num_jobs=3

# We might need binaries installed via pip, so ensure that our personal bin dir is on the PATH
export PATH=$HOME/.local/bin:$PATH

if [ -n "$JUST_JSON" ]
then
    echo "Early exit on just-json change"
    exit 0
fi

ccache --zero-stats
# Increase cache size because debug builds generate large object files
ccache -M 5G
ccache --show-stats

echo "COMPILER: $COMPILER, OS: $OS, TILES: $TILES, SOUND: $SOUND, TEST_STAGE: $TEST_STAGE"
echo "LANGUAGES: $LANGUAGES, LIBBACKTRACE: $LIBBACKTRACE, NATIVE: $NATIVE, RELEASE: $RELEASE, CROSS_COMPILATION: $CROSS_COMPILATION"

if [ "$RELEASE" = "1" ]
then
    build_type=MinSizeRel
else
    build_type=Debug
fi

TILES="${TILES:-0}"
CURSES=$((1 - TILES))

cmake_args=(
    -B build
    -G Ninja
    -DBACKTRACE=ON
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_BUILD_TYPE="$build_type"
    -DTILES="$TILES"
    -DCURSES="$CURSES"
    -DSOUND="${SOUND:-0}"
)

if [ -n "$COMPILER" ]; then
    cmake_args+=( -DCMAKE_CXX_COMPILER="$COMPILER" )
fi

if [ -n "$TEST_STAGE" ]; then
    cmake_args+=( -DJSON_FORMAT=ON )
fi

if [ "$OS" = "macos-14" ]; then
    cmake_args+=( -DCMAKE_OSX_DEPLOYMENT_TARGET=14 )
fi

cmake "${cmake_args[@]}"

if [ -n "$TEST_STAGE" ]
then
    build-scripts/lint-json.sh
    cmake --build build --target style-json-parallel --parallel "$num_jobs"
    tools/dialogue_validator.py data/json/npcs/* data/json/npcs/*/* data/json/npcs/*/*/*
fi

cmake --build build --parallel "$num_jobs"

# For CI on macOS, patch the test binary so it can find SDL3 libraries.
if [[ ! -z "$OS" && "$OS" = "macos-14" ]]
then
    test_bin="build/tests/cata_test"
    if [ "$TILES" = "1" ]; then
        test_bin="build/tests/cata_test-tiles"
    fi
    if [ -f "$test_bin" ]; then
        file "$test_bin"
        install_name_tool -add_rpath "$HOME"/Library/Frameworks "$test_bin"
    fi
fi

# vim:tw=0
