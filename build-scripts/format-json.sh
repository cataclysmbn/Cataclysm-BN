#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

json_files=()
jsonc_files=()
if (( $# > 0 )); then
    for file in "$@"; do
        file="${file#"$repo_root/"}"
        file="${file#./}"
        if [[ ! -f "$file" || "$file" == data/names/* ]]; then
            continue
        fi

        case "$file" in
            *.json)
                json_files+=( "$file" )
                ;;
            *.jsonc)
                jsonc_files+=( "$file" )
                ;;
        esac
    done
else
    while IFS= read -r -d '' file; do
        json_files+=( "$file" )
    done < <(find data -name '*.json' -type f -not -path 'data/names/*' -print0)
    while IFS= read -r -d '' file; do
        jsonc_files+=( "$file" )
    done < <(find data -name '*.jsonc' -type f -not -path 'data/names/*' -print0)
fi

if (( ${#json_files[@]} == 0 && ${#jsonc_files[@]} == 0 )); then
    exit 0
fi

json_status=0

if (( ${#json_files[@]} > 0 )); then
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

    json_formatter_status=0
    printf '%s\0' "${json_files[@]}" |
        xargs -0 -n 1 -P "$jobs" bash -c '
            formatter="$1"
            file="$2"
            file_status=0
            "$formatter" "$file" || file_status=$?
            if (( file_status == 1 )); then
                exit 0
            fi
            exit "$file_status"
        ' _ "$json_formatter" || json_formatter_status=$?
    if (( json_formatter_status != 0 )); then
        json_status="$json_formatter_status"
    fi
fi

if (( ${#jsonc_files[@]} > 0 )); then
    deno fmt --config deno.jsonc "${jsonc_files[@]}" || json_status=$?
fi

exit "$json_status"
