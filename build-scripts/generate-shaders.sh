#!/usr/bin/env bash
set -euo pipefail

shadercross="${SHADERCROSS:-shadercross}"
slangc="${SLANGC:-slangc}"
source_dir="${1:-src/shaders}"
output_dir="${2:-data/shaders}"

if [ ! -d "${source_dir}" ]; then
    echo "Shader source directory not found: ${source_dir}" >&2
    exit 1
fi

shopt -s nullglob
has_hlsl=false
for shader in "${source_dir}"/*.hlsl; do
    has_hlsl=true
done

has_slang=false
for shader in "${source_dir}"/*.slang; do
    has_slang=true
done

if { "${has_hlsl}" || "${has_slang}"; } && ! command -v "${shadercross}" >/dev/null 2>&1; then
    echo "shadercross executable not found: ${shadercross}" >&2
    exit 1
fi

if "${has_slang}" && ! command -v "${slangc}" >/dev/null 2>&1; then
    echo "slangc executable not found: ${slangc}" >&2
    exit 1
fi

mkdir -p "${output_dir}"
find "${output_dir}" -maxdepth 1 \( -name '*.spv' -o -name '*.msl' -o -name '*.dxil' \) -delete

tmp_dir=""
if "${has_slang}"; then
    tmp_dir="$(mktemp -d)"
    trap 'rm -rf "${tmp_dir}"' EXIT
fi

for shader in "${source_dir}"/*.hlsl; do
    shader_file="$(basename "${shader}")"
    shader_name="${shader_file%.hlsl}"
    if [ -f "${source_dir}/${shader_name}.slang" ]; then
        continue
    fi

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

for shader in "${source_dir}"/*.slang; do
    shader_file="$(basename "${shader}")"
    shader_name="${shader_file%.slang}"
    hlsl_out="${tmp_dir}/${shader_name}.hlsl"

    "${slangc}" "${shader}" -entry main -stage compute -profile sm_6_0 -target hlsl -o "${hlsl_out}"
    "${shadercross}" "${hlsl_out}" -s hlsl -d spirv -t compute -o "${output_dir}/${shader_name}.spv"
    "${shadercross}" "${hlsl_out}" -s hlsl -d msl -t compute -o "${output_dir}/${shader_name}.msl"
    "${shadercross}" "${hlsl_out}" -s hlsl -d dxil -t compute -o "${output_dir}/${shader_name}.dxil"
done
