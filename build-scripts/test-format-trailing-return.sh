#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT

printf '#pragma once\nint answer() { return 42; }\n' > "$scratch/sample header.h"
printf '#include "sample header.h"\nint main() { return answer(); }\n' > "$scratch/sample.cpp"
printf '[{"directory":"%s","file":"%s/sample.cpp","arguments":["clang++","-std=c++23","-c","%s/sample.cpp"]}]\n' \
    "$scratch" "$scratch" "$scratch" > "$scratch/compile_commands.json"

BUILD_PATH="$scratch" JOBS=4 build-scripts/format-trailing-return.sh \
    "$scratch/sample header.h" "$scratch/sample.cpp" > "$scratch/output" 2>&1
rg -q 'auto answer\(\) -> int' "$scratch/sample header.h"
rg -q 'auto main\(\) -> int' "$scratch/sample.cpp"

# A second run has no replacements to apply.
BUILD_PATH="$scratch" JOBS=4 build-scripts/format-trailing-return.sh \
    "$scratch/sample header.h" "$scratch/sample.cpp" >> "$scratch/output" 2>&1

# A failed analysis must not apply fixes exported by successful workers.
printf '#pragma once\nint pending() { return 1; }\n' > "$scratch/pending.h"
printf 'int broken( {\n' > "$scratch/broken.cpp"
printf '[{"directory":"%s","file":"%s/broken.cpp","arguments":["clang++","-std=c++23","-c","%s/broken.cpp"]}]\n' \
    "$scratch" "$scratch" "$scratch" > "$scratch/compile_commands.json"
if BUILD_PATH="$scratch" JOBS=4 build-scripts/format-trailing-return.sh \
    "$scratch/pending.h" "$scratch/broken.cpp" >> "$scratch/output" 2>&1; then
    echo "error: expected clang-tidy to reject invalid C++" >&2
    exit 1
fi
rg -q '^int pending\(\)' "$scratch/pending.h"

# fmt-auto turns `int *p = new int( 1 );` into `auto *p = ...` and keeps the return type.
printf 'int main() { int *p = new int( 1 ); return *p; }\n' > "$scratch/auto.cpp"
printf '[{"directory":"%s","file":"%s/auto.cpp","arguments":["clang++","-std=c++23","-c","%s/auto.cpp"]}]\n' \
    "$scratch" "$scratch" "$scratch" > "$scratch/compile_commands.json"
TIDY_CHECK=modernize-use-auto BUILD_PATH="$scratch" JOBS=4 build-scripts/format-trailing-return.sh \
    "$scratch/auto.cpp" >> "$scratch/output" 2>&1
rg -q 'auto \*p = new int' "$scratch/auto.cpp"
rg -q '^int main\(\)' "$scratch/auto.cpp"

echo "fmt-return and fmt-auto parallel analysis tests passed"
