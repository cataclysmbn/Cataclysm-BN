# Opt in with -DCMAKE_PROJECT_INCLUDE=<repo>/build-scripts/rot-coverage.cmake.
# Keep the ordinary linux-full build unchanged when this file is not selected.
option(ROT_SANITIZE "Also enable AddressSanitizer for the instrumented rot sources" OFF)

function(enable_rot_coverage)
    set(rot_options -fprofile-instr-generate -fcoverage-mapping)
    set(rot_link_options -fprofile-instr-generate)
    if(ROT_SANITIZE)
        list(APPEND rot_options -fsanitize=address -fno-omit-frame-pointer)
        list(APPEND rot_link_options -fsanitize=address)
    endif()
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/src/rot.cpp"
        "${CMAKE_SOURCE_DIR}/src/rot/rot_calculation.cpp"
        "${CMAKE_SOURCE_DIR}/src/item.cpp"
        "${CMAKE_SOURCE_DIR}/src/rot/item_rot.cpp"
        "${CMAKE_SOURCE_DIR}/src/crafting.cpp"
        "${CMAKE_SOURCE_DIR}/src/location_vector.cpp"
        "${CMAKE_SOURCE_DIR}/src/map/mapbuffer.cpp"
        DIRECTORY "${CMAKE_SOURCE_DIR}/src"
        PROPERTIES COMPILE_OPTIONS "${rot_options}"
    )
    target_link_options(cata_test-tiles PRIVATE ${rot_link_options})
    target_link_options(cataclysm-bn-tiles PRIVATE ${rot_link_options})
endfunction()

cmake_language(DEFER CALL enable_rot_coverage)
