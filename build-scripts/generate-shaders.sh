#!/usr/bin/env bash
set -euo pipefail

shadercross="${SHADERCROSS:-shadercross}"
source_dir="${1:-src/shaders}"
output_dir="${2:-data/shaders}"
formats="${3:-${SHADER_FORMATS:-spirv msl dxil dxbc}}"

if ! command -v "${shadercross}" >/dev/null 2>&1; then
    echo "shadercross executable not found: ${shadercross}" >&2
    exit 1
fi

if [ ! -d "${source_dir}" ]; then
    echo "Shader source directory not found: ${source_dir}" >&2
    exit 1
fi

mkdir -p "${output_dir}"
for format in ${formats}; do
    case "${format}" in
        spirv)
            find "${output_dir}" -maxdepth 1 -name '*.spv' -delete
            ;;
        msl)
            find "${output_dir}" -maxdepth 1 -name '*.msl' -delete
            ;;
        dxil)
            find "${output_dir}" -maxdepth 1 -name '*.dxil' -delete
            ;;
        dxbc)
            find "${output_dir}" -maxdepth 1 -name '*.dxbc' -delete
            ;;
        *)
            echo "Unsupported shader format: ${format}" >&2
            exit 1
            ;;
    esac
done

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

    for format in ${formats}; do
        case "${format}" in
            spirv)
                "${shadercross}" "${shader}" -s hlsl -d spirv -t "${stage}" -o "${output_dir}/${shader_name}.spv"
                ;;
            msl)
                "${shadercross}" "${shader}" -s hlsl -d msl -t "${stage}" -o "${output_dir}/${shader_name}.msl"
                ;;
            dxil)
                "${shadercross}" "${shader}" -s hlsl -d dxil -t "${stage}" -o "${output_dir}/${shader_name}.dxil"
                ;;
            dxbc)
                "${shadercross}" "${shader}" -s hlsl -d dxbc -t "${stage}" -o "${output_dir}/${shader_name}.dxbc"
                ;;
        esac
    done
done
