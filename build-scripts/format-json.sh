#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

json_files=()
if (( $# > 0 )); then
    for file in "$@"; do
        file="${file#"$repo_root/"}"
        file="${file#./}"
        if [[ -f "$file" && "$file" == *.json && "$file" != data/names/* && "$file" != .vscode/* ]]; then
            json_files+=( "$file" )
        fi
    done
else
    while IFS= read -r -d '' file; do
        json_files+=( "$file" )
    done < <(find data -name '*.json' -type f -not -path 'data/names/*' -print0)
fi

if (( ${#json_files[@]} == 0 )); then
    exit 0
fi

build_dir="${CATA_JSON_FORMAT_BUILD_DIR:-out/build/json-format}"
json_formatter="$build_dir/tools/format/json_formatter"

jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-}"
if [[ -z "$jobs" ]]; then
    jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)"
fi

cmake_args=(
    -S .
    -B "$build_dir"
    -DCMAKE_BUILD_TYPE=Release
    -DJSON_FORMAT=ON
    -DCATA_FORMAT_TARGETS=OFF
    -DTESTS=OFF
    -DTILES=OFF
    -DCURSES=OFF
    -DSOUND=OFF
    -DLANGUAGES=none
    -DLUA_DOCS_ON_BUILD=OFF
)

if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
    cmake_args+=( -G "$CMAKE_GENERATOR" )
elif command -v ninja >/dev/null 2>&1; then
    cmake_args+=( -G Ninja )
fi

cmake "${cmake_args[@]}"
cmake --build "$build_dir" --target json_formatter --parallel "$jobs"

json_status=0
for file in "${json_files[@]}"; do
    file_status=0
    "$json_formatter" "$file" || file_status=$?
    if (( file_status != 0 && file_status != 1 )); then
        json_status="$file_status"
    fi
done

exit "$json_status"
