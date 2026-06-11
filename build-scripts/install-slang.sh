#!/usr/bin/env bash
set -euo pipefail

version="${SLANG_VERSION:-v2026.10.2}"
version_number="${version#v}"
install_dir="${SLANG_INSTALL_DIR:-build-data/slang/current}"
download_dir="${SLANG_DOWNLOAD_DIR:-build-data/slang/downloads}"

case "$(uname -s)" in
    Linux)
        platform="linux"
        ;;
    Darwin)
        platform="macos"
        ;;
    *)
        echo "Unsupported Slang host platform: $(uname -s)" >&2
        exit 1
        ;;
esac

case "$(uname -m)" in
    x86_64|amd64)
        arch="x86_64"
        ;;
    arm64|aarch64)
        arch="aarch64"
        ;;
    *)
        echo "Unsupported Slang host architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

asset="slang-${version_number}-${platform}-${arch}.tar.gz"
url="https://github.com/shader-slang/slang/releases/download/${version}/${asset}"
archive="${download_dir}/${asset}"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT

mkdir -p "${download_dir}" "$(dirname "${install_dir}")"

if [ ! -f "${archive}" ]; then
    echo "Downloading ${asset}"
    curl --fail --location --retry 3 --output "${archive}" "${url}"
fi

tar -xzf "${archive}" -C "${tmp_dir}"

if [ -x "${tmp_dir}/bin/slangc" ]; then
    extracted_root="${tmp_dir}"
else
    extracted_root="$(find "${tmp_dir}" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
fi

if [ ! -x "${extracted_root}/bin/slangc" ]; then
    echo "Downloaded Slang archive did not contain bin/slangc" >&2
    exit 1
fi

rm -rf "${install_dir}"
mkdir -p "${install_dir}"
cp -a "${extracted_root}/." "${install_dir}/"

echo "Slang installed to ${install_dir}"
echo "Configure with: -DCATA_SLANG_ROOT=${install_dir}"
