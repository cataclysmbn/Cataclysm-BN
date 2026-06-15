#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

usage() {
    cat >&2 <<'USAGE'
usage: build-scripts/fmt.sh [--all|cpp [files...]|json [files...]|docs|lua]
USAGE
}

mode=staged
case "${1:-}" in
    "")
        ;;
    --all)
        mode=all
        shift
        ;;
    cpp|json|docs|lua)
        mode="$1"
        shift
        ;;
    *)
        usage
        exit 2
        ;;
esac

if (( $# > 0 )) && [[ "$mode" != cpp && "$mode" != json ]]; then
    usage
    exit 2
fi

if [[ "$mode" == cpp ]]; then
    build-scripts/format-cpp.sh "$@"
    exit 0
fi

if [[ "$mode" == json ]]; then
    build-scripts/format-json.sh "$@"
    exit 0
fi

if [[ "$mode" == docs ]]; then
    deno fmt
    exit 0
fi

if [[ "$mode" == lua ]]; then
    deno task dprint fmt
    exit 0
fi

cpp_files=()
json_files=()
stage_files=()

append_stage_file() {
    local file="$1"
    for staged in "${stage_files[@]}"; do
        if [[ "$staged" == "$file" ]]; then
            return
        fi
    done
    stage_files+=( "$file" )
}

if [[ "$mode" == staged ]]; then
    while IFS= read -r -d '' file; do
        file="${file#./}"
        if [[ ! -f "$file" ]]; then
            continue
        fi

        case "$file" in
            *.cpp|*.h|*.hpp)
                cpp_files+=( "$file" )
                append_stage_file "$file"
                ;;
            *.json)
                if [[ "$file" != data/names/* ]]; then
                    json_files+=( "$file" )
                    append_stage_file "$file"
                fi
                ;;
            *.md|*.ts|*.lua)
                append_stage_file "$file"
                ;;
        esac
    done < <(git diff --cached --name-only --diff-filter=ACMR -z)
fi

deno fmt
deno task dprint fmt

if [[ "$mode" == all ]]; then
    build-scripts/format-cpp.sh
    build-scripts/format-json.sh
else
    if (( ${#cpp_files[@]} > 0 )); then
        build-scripts/format-cpp.sh "${cpp_files[@]}"
    fi
    if (( ${#json_files[@]} > 0 )); then
        build-scripts/format-json.sh "${json_files[@]}"
    fi
    if (( ${#stage_files[@]} > 0 )); then
        git add -- "${stage_files[@]}"
    fi
fi
