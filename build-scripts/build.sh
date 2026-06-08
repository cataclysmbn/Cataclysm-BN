#!/bin/bash

# Build script intended for use in Github workflow

set -exo pipefail

num_jobs=3

function run_tests
{
    # The grep suppresses lines that begin with "0.0## s:", which are timing lines for tests with a very short duration.
    $WINE "$@" -d yes --use-colour yes --rng-seed time --error-format=github-action | grep -Ev "^0\.0[0-9]{2} s:"
}

# We might need binaries installed via pip, so ensure that our personal bin dir is on the PATH
export PATH=$HOME/.local/bin:$PATH

if [ -n "$DEPLOY" ]
then
    : # No-op, for now
elif [ -n "$JUST_JSON" ]
then
    echo "Early exit on just-json change"
    exit 0
fi

if [ "$CMAKE" != "1" ]
then
    echo "CMake builds are required."
    exit 1
fi

ccache --zero-stats
# Increase cache size because debug builds generate large object files
ccache -M 5G
ccache --show-stats

bin_path="./"
if [ "$RELEASE" = "1" ]
then
    build_type=MinSizeRel
    bin_path="build/tests/"
else
    build_type=Debug
fi

cmake_args=(
    -B build
    -DBACKTRACE=ON
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_BUILD_TYPE="$build_type"
    -DTILES=${TILES:-0}
    -DSOUND=${SOUND:-0}
    -DLIBBACKTRACE=${LIBBACKTRACE:-0}
)

if [ -n "$COMPILER" ]
then
    cmake_args+=( -DCMAKE_CXX_COMPILER="$COMPILER" )
fi

if [ -n "$TEST_STAGE" ]
then
    cmake_args+=( -DJSON_FORMAT=ON )
fi

cmake "${cmake_args[@]}" .

if [ -n "$TEST_STAGE" ]
then
    build-scripts/lint-json.sh
    cmake --build build --target style-json-parallel --parallel "$num_jobs"
    tools/dialogue_validator.py data/json/npcs/* data/json/npcs/*/* data/json/npcs/*/*/*
fi

cmake --build build --target translations_compile --parallel "$num_jobs"
cmake --build build --parallel "$num_jobs"

# Run regular tests
[ -f "${bin_path}cata_test" ] && run_tests "${bin_path}cata_test"
[ -f "${bin_path}cata_test-tiles" ] && run_tests "${bin_path}cata_test-tiles"

if [ -n "$TEST_STAGE" ]
then
    # Run the tests one more time, without actually running any tests, just to verify that all
    # the mod data can be successfully loaded

    # Use a blacklist of mods that currently fail to load cleanly.  Hopefully this list will
    # shrink over time.
    blacklist=build-scripts/mod_test_blacklist
    if [ "$LUA" == "1" ]
    then
        do_lua="1"
    else
        do_lua="0"
    fi
    mods="$(./build-scripts/get_all_mods.py $blacklist $do_lua)"
    run_tests "${bin_path}cata_test" --user-dir=all_modded --mods="$mods" '~*'
fi

ccache --show-stats
# Shrink the ccache back down to 2GB in preperation for pushing to shared storage.
ccache -M 2G
ccache -c

# vim:tw=0
