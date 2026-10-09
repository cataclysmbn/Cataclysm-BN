if (BUILD_TESTING)
    # Register deterministic RNG trace probes only in the shared engine source
    # and its focused test source. Non-test builds remain uninstrumented.
    set_property(SOURCE "${CMAKE_SOURCE_DIR}/src/rng.cpp"
        DIRECTORY "${CMAKE_SOURCE_DIR}/src"
        APPEND PROPERTY COMPILE_DEFINITIONS CATA_RNG_TRACE_TESTING)
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/deterministic_rng_test.cpp"
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        APPEND PROPERTY COMPILE_DEFINITIONS CATA_RNG_TRACE_TESTING)

    # Deterministic pre-engagement allocation/interruption faults, test builds only.
    set_property(SOURCE "${CMAKE_SOURCE_DIR}/src/evaluation_decision.cpp"
        DIRECTORY "${CMAKE_SOURCE_DIR}/src"
        APPEND PROPERTY COMPILE_DEFINITIONS CATA_EVALUATION_DECISION_TESTING)
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/evaluation_decision_test.cpp"
        APPEND PROPERTY COMPILE_DEFINITIONS
            CATA_EVALUATION_DECISION_TESTING CATA_TRANSLATION_COPY_TESTING CATA_LANGUAGE_TESTING)
    # Real catalogue fixture publication is available only in test builds.
    set_property(SOURCE "${CMAKE_SOURCE_DIR}/src/language.cpp"
        DIRECTORY "${CMAKE_SOURCE_DIR}/src"
        APPEND PROPERTY COMPILE_DEFINITIONS CATA_LANGUAGE_TESTING)
    # Behavior-neutral completed-copy observation for the pre-admission translated-label RED.
    set_property(SOURCE "${CMAKE_SOURCE_DIR}/src/translations.cpp"
        DIRECTORY "${CMAKE_SOURCE_DIR}/src"
        APPEND PROPERTY COMPILE_DEFINITIONS CATA_TRANSLATION_COPY_TESTING)

    # Explicit opt-in requires a real private graphical session (for example Xvfb).
    # Native message controls remain runnable by ordinary headless CTest.
    option(CATA_NATIVE_PREVIEW_GRAPHICS "Enable real graphical character-preview CTest" OFF)
    file(GLOB CATACLYSM_BN_TEST_SOURCES
            ${CMAKE_SOURCE_DIR}/tests/*.cpp)

    # Exercise the actual Android publication code in input.h/input.cpp on a host,
    # isolated from all host-layout engine objects. This is not an Android product build.
    add_executable(cata_test-input-metadata-android
        input_metadata_android_test.cpp ${CMAKE_SOURCE_DIR}/src/input.cpp)
    target_include_directories(cata_test-input-metadata-android PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(cata_test-input-metadata-android PRIVATE
        __ANDROID__ CATA_INPUT_LIFECYCLE_TEST)
    add_test(NAME cata_test-input-metadata-android
        COMMAND $<TARGET_FILE:cata_test-input-metadata-android>)

    # Enabling benchmarks
    add_definitions(-DCATCH_CONFIG_ENABLE_BENCHMARKING)

    # TODO: build MO files required for tests

    # Ordinary tests link fake_messages.cpp.  Preview purity needs the real message
    # authority, so keep a small native-message harness rather than changing those stubs.
    function(add_native_preview_test target_name client_target client_definition)
        add_executable(${target_name}
            test_main.cpp character_preview_test.cpp
            preview_callback_capability_test.cpp evaluation_decision_test.cpp
            state_helpers.cpp player_helpers.cpp map_helpers.cpp
            options_helpers.cpp tile_helpers.cpp
            ${CMAKE_SOURCE_DIR}/src/messages.cpp)
        target_compile_definitions(${target_name} PRIVATE
            CATA_NATIVE_PREVIEW_TEST ${client_definition})
        if (client_definition STREQUAL "TILES" AND DYNAMIC_ATLAS)
            target_compile_definitions(${target_name} PRIVATE DYNAMIC_ATLAS)
        endif ()
        target_link_libraries(${target_name} PRIVATE
            cataclysm-bn-engine cataclysm-bn-client-common
            ${client_target} cataclysm-bn-client-audio)
        target_compile_options(${target_name} PRIVATE ${CATCH2_COMPILE_OPTIONS})
        add_test(NAME ${target_name}
            COMMAND $<TARGET_FILE:${target_name}> "~[tiles]" --rng-seed 424242
                --user-dir=${CMAKE_CURRENT_BINARY_DIR}/${target_name}-profile/
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        if (client_definition STREQUAL "TILES")
            add_test(NAME ${target_name}-graphics
                COMMAND $<TARGET_FILE:${target_name}> "[tiles]" --rng-seed 424242
                    --user-dir=${CMAKE_CURRENT_BINARY_DIR}/${target_name}-graphics-profile/
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
            set_tests_properties(${target_name}-graphics PROPERTIES LABELS "graphics")
            if (NOT CATA_NATIVE_PREVIEW_GRAPHICS)
                set_tests_properties(${target_name}-graphics PROPERTIES DISABLED TRUE)
            endif ()
        endif ()
        if (COMMAND cata_copy_windows_lavapipe_runtime)
            cata_copy_windows_lavapipe_runtime(${target_name})
        endif ()
    endfunction()

    # This needs to include catch.h with different macro switch
    set_source_files_properties(test_main.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

    # Catch2 v2 uses __COUNTER__, which Clang 22 reports under -Wpedantic.
    set(CATCH2_COMPILE_OPTIONS $<$<CXX_COMPILER_ID:Clang>:-Wno-c2y-extensions>)

    if (TILES)
        add_executable(cata_test-tiles ${CATACLYSM_BN_TEST_SOURCES})
        target_compile_definitions(cata_test-tiles PRIVATE TILES)
        if (DYNAMIC_ATLAS)
            target_compile_definitions(cata_test-tiles PRIVATE DYNAMIC_ATLAS)
        endif ()
        target_link_libraries(cata_test-tiles PRIVATE
            cataclysm-bn-engine cataclysm-bn-client-common
            cataclysm-bn-tiles-client)
        target_link_libraries(cata_test-tiles PRIVATE cataclysm-bn-client-audio)
        target_compile_options(cata_test-tiles PRIVATE ${CATCH2_COMPILE_OPTIONS})

        add_native_preview_test(cata_test-preview-tiles cataclysm-bn-tiles-client TILES)

        add_test(
            NAME test-tiles
            COMMAND sh -c "$<TARGET_FILE:cata_test-tiles> --rng-seed time --user-dir=${CMAKE_CURRENT_BINARY_DIR}/test-tiles-profile/"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        if (NOT "${CMAKE_EXPORT_COMPILE_COMMANDS}")
            target_precompile_headers(cata_test-tiles PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/pch/tests-pch.hpp)
        endif ()

        set_target_properties( cata_test-tiles PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" )
        if (COMMAND cata_copy_windows_lavapipe_runtime)
            cata_copy_windows_lavapipe_runtime(cata_test-tiles)
        endif()
    endif ()

    if (CURSES)
        add_executable(cata_test ${CATACLYSM_BN_TEST_SOURCES})
        target_link_libraries(cata_test PRIVATE
            cataclysm-bn-engine cataclysm-bn-client-common
            cataclysm-bn-curses-client)
        target_link_libraries(cata_test PRIVATE cataclysm-bn-client-audio)
        target_compile_options(cata_test PRIVATE ${CATCH2_COMPILE_OPTIONS})

        add_test(
            NAME test
            COMMAND sh -c "$<TARGET_FILE:cata_test> --rng-seed time --user-dir=${CMAKE_CURRENT_BINARY_DIR}/test-profile/"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        if (NOT "${CMAKE_EXPORT_COMPILE_COMMANDS}")
            target_precompile_headers(cata_test PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/pch/tests-pch.hpp)
        endif ()
        if (COMMAND cata_copy_windows_lavapipe_runtime)
            cata_copy_windows_lavapipe_runtime(cata_test)
        endif()
    endif ()

    if (MCP)
        # Real native messages and bound objects for command-boundary acquisition.
        add_executable(cata_test-perception-mcp
            test_main.cpp map_helpers.cpp player_helpers.cpp state_helpers.cpp
            options_helpers.cpp map_perception_bionics_boundary_test.cpp
            ${CMAKE_SOURCE_DIR}/src/messages.cpp)
        target_compile_definitions(cata_test-perception-mcp PRIVATE CATA_MCP)
        target_link_libraries(cata_test-perception-mcp PRIVATE
            cataclysm-bn-engine cataclysm-bn-client-common cataclysm-bn-mcp-client
            cataclysm-bn-client-audio cataclysm-bn-mcp-protocol)
        target_compile_options(cata_test-perception-mcp PRIVATE ${CATCH2_COMPILE_OPTIONS})
        add_test(NAME test-perception-mcp
            COMMAND $<TARGET_FILE:cata_test-perception-mcp>
                "[map_perception_bionics_boundary]" --rng-seed 424242
                --user-dir=${CMAKE_CURRENT_BINARY_DIR}/cata_test-perception-mcp-profile/
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        add_executable(cata_test-mcp ${CATACLYSM_BN_TEST_SOURCES})
        target_compile_definitions(cata_test-mcp PRIVATE CATA_MCP)
        target_link_libraries(cata_test-mcp PRIVATE
            cataclysm-bn-engine cataclysm-bn-client-common
            cataclysm-bn-mcp-client)
        target_link_libraries(cata_test-mcp PRIVATE cataclysm-bn-client-audio)
        target_link_libraries(cata_test-mcp PRIVATE cataclysm-bn-mcp-protocol)
        target_compile_options(cata_test-mcp PRIVATE ${CATCH2_COMPILE_OPTIONS})
        add_native_preview_test(cata_test-preview-mcp cataclysm-bn-mcp-client CATA_MCP)

        # Manager controls need real message authority, without replacing the fuel suite below.
        add_executable(cata_test-bionics-mcp
            test_main.cpp bionics_manager_test.cpp
            state_helpers.cpp player_helpers.cpp map_helpers.cpp
            options_helpers.cpp tile_helpers.cpp
            ${CMAKE_SOURCE_DIR}/src/messages.cpp)
        target_compile_definitions(cata_test-bionics-mcp PRIVATE
            CATA_MCP CATA_BIONICS_NATIVE_TEST)
        target_link_libraries(cata_test-bionics-mcp PRIVATE
            cataclysm-bn-engine cataclysm-bn-client-common
            cataclysm-bn-mcp-client cataclysm-bn-client-audio cataclysm-bn-mcp-protocol)
        target_compile_options(cata_test-bionics-mcp PRIVATE ${CATCH2_COMPILE_OPTIONS})
        add_test(NAME test-bionics-mcp
            COMMAND $<TARGET_FILE:cata_test-bionics-mcp>
                "[bionics_manager]" --rng-seed 424242
                --user-dir=${CMAKE_CURRENT_BINARY_DIR}/cata_test-bionics-mcp-profile/
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(test-bionics-mcp PROPERTIES LABELS "bionics;native_messages")
        if (COMMAND cata_copy_windows_lavapipe_runtime)
            cata_copy_windows_lavapipe_runtime(cata_test-bionics-mcp)
        endif ()
        # The real debug prompt needs native drawing and message authority, not message stubs.
        add_executable(cata_test-debug-mcp
            test_main.cpp client_debug_interaction_test.cpp
            state_helpers.cpp player_helpers.cpp map_helpers.cpp
            options_helpers.cpp tile_helpers.cpp
            ${CMAKE_SOURCE_DIR}/src/messages.cpp)
        target_compile_definitions(cata_test-debug-mcp PRIVATE CATA_MCP CATA_DEBUG_NATIVE_TEST)
        target_link_libraries(cata_test-debug-mcp PRIVATE
            cataclysm-bn-engine cataclysm-bn-client-common
            cataclysm-bn-mcp-client cataclysm-bn-client-audio cataclysm-bn-mcp-protocol)
        target_compile_options(cata_test-debug-mcp PRIVATE ${CATCH2_COMPILE_OPTIONS})
        add_test(NAME test-debug-mcp
            COMMAND $<TARGET_FILE:cata_test-debug-mcp> "[debug_prompt]" --rng-seed 424242
                --user-dir=${CMAKE_CURRENT_BINARY_DIR}/cata_test-debug-mcp-profile/
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(test-debug-mcp PROPERTIES LABELS "debug;native_messages" TIMEOUT 240)

        # Core fuel-observation tests use native messages and native manager actions.
        add_executable(cata_test-bionics-fuel-mcp
            test_main.cpp bionics_fuel_observation_test.cpp
            state_helpers.cpp player_helpers.cpp map_helpers.cpp
            options_helpers.cpp tile_helpers.cpp
            ${CMAKE_SOURCE_DIR}/src/messages.cpp)
        target_compile_definitions(cata_test-bionics-fuel-mcp PRIVATE
            CATA_MCP CATA_BIONICS_FUEL_NATIVE_TEST)
        target_link_libraries(cata_test-bionics-fuel-mcp PRIVATE
            cataclysm-bn-engine cataclysm-bn-client-common
            cataclysm-bn-mcp-client cataclysm-bn-client-audio cataclysm-bn-mcp-protocol)
        target_compile_options(cata_test-bionics-fuel-mcp PRIVATE ${CATCH2_COMPILE_OPTIONS})
        add_test(NAME test-bionics-fuel-mcp
            COMMAND $<TARGET_FILE:cata_test-bionics-fuel-mcp>
                "[bionics_remote_purity],[bionics_remote_controls]" --rng-seed 424242
                --user-dir=${CMAKE_CURRENT_BINARY_DIR}/cata_test-bionics-fuel-mcp-profile/
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(test-bionics-fuel-mcp PROPERTIES LABELS "bionics;native_messages")
        if (COMMAND cata_copy_windows_lavapipe_runtime)
            cata_copy_windows_lavapipe_runtime(cata_test-bionics-fuel-mcp)
        endif ()

        add_test(
            NAME test-mcp
            COMMAND sh -c "$<TARGET_FILE:cata_test-mcp> --rng-seed time --user-dir=${CMAKE_CURRENT_BINARY_DIR}/test-mcp-profile/"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        if (NOT "${CMAKE_EXPORT_COMPILE_COMMANDS}")
            target_precompile_headers(cata_test-mcp PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/pch/tests-pch.hpp)
        endif ()
    endif ()
endif ()
