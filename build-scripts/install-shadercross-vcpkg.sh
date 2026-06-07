#!/usr/bin/env bash
set -euo pipefail

vcpkg_root="${1:-${VCPKG_ROOT:-}}"
if [ -z "${vcpkg_root}" ]; then
    if [ -n "${RUNNER_TEMP:-}" ]; then
        vcpkg_root="${RUNNER_TEMP}/vcpkg-shadercross"
    else
        vcpkg_root="${PWD}/build-data/vcpkg-shadercross"
    fi
fi

triplet="${2:-}"
if [ -z "${triplet}" ]; then
    case "$(uname -s)" in
        Linux)
            triplet="x64-linux"
            ;;
        Darwin)
            echo "vcpkg sdl3-shadercross is not supported on macOS because directx-dxc is unsupported there." >&2
            echo "Use generated shader artifacts for macOS builds instead." >&2
            exit 1
            ;;
        *)
            echo "Unsupported host platform for automatic shadercross install: $(uname -s)" >&2
            exit 1
            ;;
    esac
fi

missing_tools=()
for tool in autoconf automake; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        missing_tools+=("${tool}")
    fi
done

if ! command -v libtoolize >/dev/null 2>&1 && ! command -v glibtoolize >/dev/null 2>&1; then
    missing_tools+=("libtoolize")
fi

if [ ${#missing_tools[@]} -ne 0 ]; then
    echo "Missing shadercross vcpkg prerequisites: ${missing_tools[*]}" >&2
    echo "Install autoconf, autoconf-archive, automake, libtool, and libltdl-dev before running this script." >&2
    exit 1
fi

if [ ! -d "${vcpkg_root}" ]; then
    git clone --depth 1 https://github.com/microsoft/vcpkg "${vcpkg_root}"
elif [ ! -x "${vcpkg_root}/vcpkg" ] && [ ! -f "${vcpkg_root}/bootstrap-vcpkg.sh" ]; then
    echo "Existing vcpkg root is not a vcpkg checkout: ${vcpkg_root}" >&2
    exit 1
fi

bash "${vcpkg_root}/bootstrap-vcpkg.sh" -disableMetrics
"${vcpkg_root}/vcpkg" install "sdl3-shadercross:${triplet}"

shadercross_dir="${vcpkg_root}/installed/${triplet}/tools/sdl3-shadercross"
shadercross_exe="${shadercross_dir}/shadercross"
if [ ! -x "${shadercross_exe}" ]; then
    echo "shadercross was not found after vcpkg install: ${shadercross_exe}" >&2
    exit 1
fi

echo "shadercross=${shadercross_exe}"

if [ -n "${GITHUB_ENV:-}" ]; then
    {
        echo "SHADERCROSS=${shadercross_exe}"
        echo "SHADERCROSS_DIR=${shadercross_dir}"
    } >> "${GITHUB_ENV}"
fi

if [ -n "${GITHUB_PATH:-}" ]; then
    echo "${shadercross_dir}" >> "${GITHUB_PATH}"
fi
