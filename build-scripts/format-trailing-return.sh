#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

if (( $# == 0 )); then
    echo "usage: just fmt-return <files or dirs>" >&2
    exit 2
fi

if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "error: clang-tidy executable was not found" >&2
    exit 1
fi

build_path="${BUILD_PATH:-out/build/linux-full}"
if [[ ! -f "$build_path/compile_commands.json" ]]; then
    echo "error: compilation database not found at $build_path/compile_commands.json" >&2
    echo "hint: configure it with 'cmake --preset linux-full' or set BUILD_PATH" >&2
    exit 1
fi

files_file="$(mktemp)"
trap 'rm -f "$files_file"' EXIT

for path in "$@"; do
    if [[ -d "$path" ]]; then
        find "$path" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0 >> "$files_file"
    elif [[ -f "$path" ]]; then
        case "$path" in
            *.cpp|*.h|*.hpp)
                printf '%s\0' "$path" >> "$files_file"
                ;;
            *)
                echo "error: unsupported C++ file: $path" >&2
                exit 2
                ;;
        esac
    else
        echo "error: path does not exist: $path" >&2
        exit 2
    fi
done

sort -zu -o "$files_file" "$files_file"

if [[ ! -s "$files_file" ]]; then
    echo "error: no C++ files found" >&2
    exit 2
fi

jobs="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}"
tidy_status=0
xargs -0 -n 1 -P "$jobs" clang-tidy \
    --quiet \
    --fix \
    --checks='-*,modernize-use-trailing-return-type' \
    --config='{CheckOptions: {modernize-use-trailing-return-type.TransformLambdas: none}}' \
    --header-filter='^$' \
    -p "$build_path" \
    < "$files_file" || tidy_status=$?

files=()
mapfile -d '' files < "$files_file"
build-scripts/format-cpp.sh "${files[@]}"

exit "$tidy_status"
