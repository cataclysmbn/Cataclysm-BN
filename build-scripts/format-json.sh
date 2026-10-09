#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd -P "$repo_root"
repo_root="$PWD"

json_files=()
if (( $# > 0 )); then
    for file in "$@"; do
        if [[ ! -f "$file" || "$file" != *.json ]]; then
            continue
        fi
        links=()
        while true; do
            # Resolve parent segments and directory aliases before checking the file target.
            # The ending slash preserves directory names ending in newlines.
            parent="${file%/*}"
            if [[ "$parent" == "$file" ]]; then
                parent=.
            fi
            parent="$(CDPATH= cd -P -- "$parent" 2>/dev/null && printf '%s/' "$PWD")" || continue 2
            file="$parent${file##*/}"
            if [[ ! -L "$file" ]]; then
                break
            fi
            # Detect repeated links without imposing an arbitrary chain-length limit.
            for link in ${links[@]+"${links[@]}"}; do
                if [[ "$file" == "$link" ]]; then
                    continue 3
                fi
            done
            links+=( "$file" )
            if ! command -v readlink >/dev/null 2>&1; then
                echo "error: readlink is required to resolve explicit JSON symlinks" >&2
                exit 1
            fi
            # Use only portable readlink, not -f; preserve newlines in the link text.
            target="$(readlink "$file" && printf '.')" || continue 2
            target="${target%$'\n.'}"
            if [[ "$target" == /* ]]; then
                file="$target"
            else
                file="$parent$target"
            fi
        done
        if [[ -f "$file" && "$file" == "$repo_root/data/"* && "$file" != "$repo_root/data/names/"* ]]; then
            json_files+=( "${file#"$repo_root/"}" )
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
