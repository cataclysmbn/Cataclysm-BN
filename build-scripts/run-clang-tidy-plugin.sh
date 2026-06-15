#!/usr/bin/env bash

set -exo pipefail

BUILD_TYPE=${BUILD_TYPE:-"RelWithDeb"}
NUM_JOBS=${NUM_JOBS:-$(nproc)}
BUILD_PATH=${BUILD_PATH:-"build"}
COMPILER=${COMPILER:-"/usr/bin/clang++"}

echo "Using bash version $BASH_VERSION with $NUM_JOBS jobs"
echo "Using build files at $BUILD_PATH"

AFFECTED_FILES=${AFFECTED_FILES:-"affected_files.txt"}
FILTERED_AFFECTED_FILES=$(mktemp)
trap 'rm -f "$FILTERED_AFFECTED_FILES"' EXIT

grep -E '^(src|tests)/.*\.(cpp|h|hpp)$' "$AFFECTED_FILES" > "$FILTERED_AFFECTED_FILES" || true

if ! grep -q '[^[:space:]]' "$FILTERED_AFFECTED_FILES"
then
  echo "No files to analyze."
  exit 0
else
  echo "Analyzing $(wc -l < "$FILTERED_AFFECTED_FILES") files"
  # shellcheck disable=SC2002
  # cat "$AFFECTED_FILES" | parallel  -j"$NUM_JOBS" --no-notice -a ./build-scripts/clang-tidy-wrapper.sh {}
  set +e # so we can run all the checks
  < "$FILTERED_AFFECTED_FILES" xargs -n1 -P"$NUM_JOBS" ./build-scripts/clang-tidy-wrapper.sh -quiet
fi
