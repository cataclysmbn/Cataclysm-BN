set(CATA_SLANG_PROBE_DXIL OFF CACHE BOOL "Also compile the Slang probe to DXIL.")

function(cata_find_slang)
    unset(CATA_SLANGC_EXE CACHE)
    unset(CATA_SLANG_CPP_TYPES_DIR CACHE)

    if (CATA_SLANGC)
        set(CATA_SLANGC_EXE "${CATA_SLANGC}" CACHE FILEPATH "Resolved slangc path." FORCE)
    elseif (CATA_SLANG_ROOT)
        find_program(CATA_SLANGC_EXE
            NAMES slangc slangc.exe
            HINTS "${CATA_SLANG_ROOT}/bin"
            NO_DEFAULT_PATH)
    else()
        find_program(CATA_SLANGC_EXE NAMES slangc slangc.exe)
    endif()

    if (CATA_SLANG_INCLUDE_DIR)
        set(CATA_SLANG_CPP_TYPES_DIR "${CATA_SLANG_INCLUDE_DIR}" CACHE PATH
            "Resolved Slang generated C++ support header directory." FORCE)
    elseif (CATA_SLANG_ROOT)
        find_path(CATA_SLANG_CPP_TYPES_DIR
            NAMES slang-cpp-types.h
            HINTS "${CATA_SLANG_ROOT}/include"
            NO_DEFAULT_PATH)
    else()
        find_path(CATA_SLANG_CPP_TYPES_DIR NAMES slang-cpp-types.h)
    endif()
endfunction()

cata_find_slang()

if (BUILD_SLANG AND (NOT CATA_SLANGC_EXE OR NOT CATA_SLANG_CPP_TYPES_DIR))
    set(CATA_SLANG_AUTO_ROOT "${CMAKE_BINARY_DIR}/_deps/slang/current")
    set(CATA_SLANG_AUTO_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps/slang/downloads")
    message(STATUS "Slang not found; downloading prebuilt Slang to ${CATA_SLANG_AUTO_ROOT}")

    if (WIN32)
        find_program(CATA_POWERSHELL_EXE NAMES pwsh powershell powershell.exe REQUIRED)
        execute_process(
            COMMAND "${CATA_POWERSHELL_EXE}" -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_SOURCE_DIR}/build-scripts/install-slang.ps1"
                    -InstallDir "${CATA_SLANG_AUTO_ROOT}"
                    -DownloadDir "${CATA_SLANG_AUTO_DOWNLOAD_DIR}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE CATA_SLANG_INSTALL_RESULT)
    else()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env
                    "SLANG_INSTALL_DIR=${CATA_SLANG_AUTO_ROOT}"
                    "SLANG_DOWNLOAD_DIR=${CATA_SLANG_AUTO_DOWNLOAD_DIR}"
                    bash "${CMAKE_SOURCE_DIR}/build-scripts/install-slang.sh"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE CATA_SLANG_INSTALL_RESULT)
    endif()

    if (NOT CATA_SLANG_INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "Slang download failed with exit code ${CATA_SLANG_INSTALL_RESULT}")
    endif()

    set(CATA_SLANG_ROOT "${CATA_SLANG_AUTO_ROOT}" CACHE PATH
        "Directory containing a Slang release with bin/slangc and include/slang-cpp-types." FORCE)
    cata_find_slang()
endif()

if (NOT CATA_SLANGC_EXE)
    message(FATAL_ERROR
        "CATA_SLANG_PROBE requires slangc. Set CATA_SLANG_ROOT or CATA_SLANGC, "
        "or enable BUILD_SLANG.")
endif()

if (NOT CATA_SLANG_CPP_TYPES_DIR)
    message(FATAL_ERROR
        "CATA_SLANG_PROBE requires slang-cpp-types.h. Set CATA_SLANG_ROOT "
        "or CATA_SLANG_INCLUDE_DIR, or enable BUILD_SLANG.")
endif()

get_filename_component(CATA_SLANG_BIN_DIR "${CATA_SLANGC_EXE}" DIRECTORY)
if (WIN32)
    set(CATA_SLANGC_COMMAND "${CATA_SLANGC_EXE}")
elseif (APPLE)
    set(CATA_SLANGC_COMMAND
        "${CMAKE_COMMAND}" -E env
        "DYLD_LIBRARY_PATH=${CATA_SLANG_BIN_DIR}:$ENV{DYLD_LIBRARY_PATH}"
        "${CATA_SLANGC_EXE}")
else()
    set(CATA_SLANGC_COMMAND
        "${CMAKE_COMMAND}" -E env
        "LD_LIBRARY_PATH=${CATA_SLANG_BIN_DIR}:$ENV{LD_LIBRARY_PATH}"
        "${CATA_SLANGC_EXE}")
endif()

set(CATA_SLANG_PROBE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/max_probe.slang")
set(CATA_SLANG_PROBE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(CATA_SLANG_PROBE_CPU_CPP "${CATA_SLANG_PROBE_GENERATED_DIR}/max_probe.cpp")
set(CATA_SLANG_PROBE_SPV "${CATA_SLANG_PROBE_GENERATED_DIR}/max_probe.spv")
set(CATA_SLANG_PROBE_MSL "${CATA_SLANG_PROBE_GENERATED_DIR}/max_probe.metal")
set(CATA_SLANG_PROBE_OUTPUTS
    "${CATA_SLANG_PROBE_CPU_CPP}"
    "${CATA_SLANG_PROBE_SPV}"
    "${CATA_SLANG_PROBE_MSL}")

add_custom_command(
    OUTPUT "${CATA_SLANG_PROBE_CPU_CPP}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CATA_SLANG_PROBE_GENERATED_DIR}"
    COMMAND ${CATA_SLANGC_COMMAND}
            "${CATA_SLANG_PROBE_SOURCE}"
            -entry max_probe_main
            -stage compute
            -target cpp
            -DCATA_SLANG_CPU=1
            -o "${CATA_SLANG_PROBE_CPU_CPP}"
    DEPENDS "${CATA_SLANG_PROBE_SOURCE}"
    COMMENT "Compiling Slang max probe -> CPU C++"
    VERBATIM)

add_custom_command(
    OUTPUT "${CATA_SLANG_PROBE_SPV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CATA_SLANG_PROBE_GENERATED_DIR}"
    COMMAND ${CATA_SLANGC_COMMAND}
            "${CATA_SLANG_PROBE_SOURCE}"
            -entry max_probe_main
            -stage compute
            -profile sm_6_0
            -target spirv
            -o "${CATA_SLANG_PROBE_SPV}"
    DEPENDS "${CATA_SLANG_PROBE_SOURCE}"
    COMMENT "Compiling Slang max probe -> SPIR-V"
    VERBATIM)

add_custom_command(
    OUTPUT "${CATA_SLANG_PROBE_MSL}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CATA_SLANG_PROBE_GENERATED_DIR}"
    COMMAND ${CATA_SLANGC_COMMAND}
            "${CATA_SLANG_PROBE_SOURCE}"
            -entry max_probe_main
            -stage compute
            -profile sm_6_0
            -target metal
            -o "${CATA_SLANG_PROBE_MSL}"
    DEPENDS "${CATA_SLANG_PROBE_SOURCE}"
    COMMENT "Compiling Slang max probe -> Metal"
    VERBATIM)

if (CATA_SLANG_PROBE_DXIL)
    set(CATA_SLANG_PROBE_DXIL_OUT "${CATA_SLANG_PROBE_GENERATED_DIR}/max_probe.dxil")
    add_custom_command(
        OUTPUT "${CATA_SLANG_PROBE_DXIL_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CATA_SLANG_PROBE_GENERATED_DIR}"
        COMMAND ${CATA_SLANGC_COMMAND}
                "${CATA_SLANG_PROBE_SOURCE}"
                -entry max_probe_main
                -stage compute
                -profile sm_6_0
                -target dxil
                -o "${CATA_SLANG_PROBE_DXIL_OUT}"
        DEPENDS "${CATA_SLANG_PROBE_SOURCE}"
        COMMENT "Compiling Slang max probe -> DXIL"
        VERBATIM)
    list(APPEND CATA_SLANG_PROBE_OUTPUTS "${CATA_SLANG_PROBE_DXIL_OUT}")
endif()

add_custom_target(cata_slang_probe_codegen
    DEPENDS ${CATA_SLANG_PROBE_OUTPUTS})

add_executable(cata_slang_max_probe
    max_probe_runner.cpp)
add_dependencies(cata_slang_max_probe cata_slang_probe_codegen)
target_include_directories(cata_slang_max_probe PRIVATE
    "${CATA_SLANG_PROBE_GENERATED_DIR}"
    "${CATA_SLANG_CPP_TYPES_DIR}")
set_target_properties(cata_slang_max_probe PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

add_custom_target(cata_slang_probe
    COMMAND "$<TARGET_FILE:cata_slang_max_probe>"
    DEPENDS cata_slang_max_probe
    COMMENT "Running Slang CPU fallback feasibility probe"
    VERBATIM)
