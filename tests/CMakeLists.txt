if (BUILD_TESTING)
    file(GLOB CATACLYSM_BN_TEST_SOURCES
            ${CMAKE_SOURCE_DIR}/tests/*.cpp)

    # Enabling benchmarks
    add_definitions(-DCATCH_CONFIG_ENABLE_BENCHMARKING)

    # TODO: build MO files required for tests

    # This needs to include catch.h with different macro switch
    set_source_files_properties(test_main.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

    # Catch2 v2 uses __COUNTER__, which Clang 22 reports under -Wpedantic.
    set(CATCH2_COMPILE_OPTIONS $<$<CXX_COMPILER_ID:Clang>:-Wno-c2y-extensions>)

    # What only the protocol engine has is tested only against it.
    if (NOT MCP)
        list(FILTER CATACLYSM_BN_TEST_SOURCES EXCLUDE REGEX "/tests/(client_(command|input|interaction[^/]*|memory|screen_support|text_interaction)|drawn_screen|editmap_screen|engine_client_[^/]*|iteminfo_observation|bionics_fuel_observation|map_perception_[^/]*|native_(info_render|overmap_render)|overmap_screen|screen_[^/]*|settings_native_pin|spell_menu|step_cost|target_screen|vending_screen|zones_screen)_test\\.cpp$")
    endif ()

    if (TILES)
        add_executable(cata_test-tiles ${CATACLYSM_BN_TEST_SOURCES})
        target_link_libraries(cata_test-tiles PRIVATE cataclysm-bn-tiles-common)
        target_compile_options(cata_test-tiles PRIVATE ${CATCH2_COMPILE_OPTIONS})

        add_test(
            NAME test-tiles
            COMMAND sh -c "$<TARGET_FILE:cata_test-tiles> --rng-seed time"
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
        target_link_libraries(cata_test PRIVATE cataclysm-bn-common)
        target_compile_options(cata_test PRIVATE ${CATCH2_COMPILE_OPTIONS})

        add_test(
            NAME test
            COMMAND sh -c "$<TARGET_FILE:cata_test> --rng-seed time"
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
        # These two need the real message log; the others link fake_messages.cpp.
        set(CATA_TEST_REAL_MESSAGES map_perception_bionics_boundary_test.cpp bionics_fuel_observation_test.cpp)
        list(FILTER CATACLYSM_BN_TEST_SOURCES EXCLUDE REGEX "/tests/(map_perception_bionics_boundary|bionics_fuel_observation)_test\\.cpp$")
        add_executable(cata_test-mcp-messages test_main.cpp map_helpers.cpp player_helpers.cpp
            state_helpers.cpp options_helpers.cpp tile_helpers.cpp ${CATA_TEST_REAL_MESSAGES}
            ${CMAKE_SOURCE_DIR}/src/messages.cpp)
        target_link_libraries(cata_test-mcp-messages PRIVATE cataclysm-bn-mcp-common)
        target_compile_options(cata_test-mcp-messages PRIVATE ${CATCH2_COMPILE_OPTIONS})
        add_test(
            NAME test-mcp-messages
            COMMAND sh -c "$<TARGET_FILE:cata_test-mcp-messages> --rng-seed 424242"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        add_executable(cata_test-mcp ${CATACLYSM_BN_TEST_SOURCES})
        target_link_libraries(cata_test-mcp PRIVATE cataclysm-bn-mcp-common)
        target_compile_options(cata_test-mcp PRIVATE ${CATCH2_COMPILE_OPTIONS})

        add_test(
            NAME test-mcp
            COMMAND sh -c "$<TARGET_FILE:cata_test-mcp> --rng-seed time"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        if (NOT "${CMAKE_EXPORT_COMPILE_COMMANDS}")
            target_precompile_headers(cata_test-mcp PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/pch/tests-pch.hpp)
        endif ()
    endif ()
endif ()
