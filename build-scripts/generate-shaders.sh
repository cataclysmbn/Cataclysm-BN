#!/usr/bin/env bash
set -euo pipefail

shadercross="${SHADERCROSS:-shadercross}"
source_dir="${1:-src/shaders}"
output_dir="${2:-data/shaders}"

if ! command -v "${shadercross}" >/dev/null 2>&1; then
    echo "shadercross executable not found: ${shadercross}" >&2
    exit 1
fi

if [ ! -d "${source_dir}" ]; then
    echo "Shader source directory not found: ${source_dir}" >&2
    exit 1
fi

mkdir -p "${output_dir}"
find "${output_dir}" -maxdepth 1 \( -name '*.spv' -o -name '*.msl' -o -name '*.dxil' \) -delete

shopt -s nullglob
for shader in "${source_dir}"/*.hlsl; do
    shader_file="$(basename "${shader}")"
    shader_name="${shader_file%.hlsl}"
    stage="compute"
    if [[ "${shader_name}" == *_vertex ]]; then
        stage="vertex"
    elif [[ "${shader_name}" == *_fragment ]]; then
        stage="fragment"
    fi

    "${shadercross}" "${shader}" -s hlsl -d spirv -t "${stage}" -o "${output_dir}/${shader_name}.spv"
    "${shadercross}" "${shader}" -s hlsl -d msl -t "${stage}" -o "${output_dir}/${shader_name}.msl"
    "${shadercross}" "${shader}" -s hlsl -d dxil -t "${stage}" -o "${output_dir}/${shader_name}.dxil"
done
