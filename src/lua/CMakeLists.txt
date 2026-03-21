file(GLOB LUA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
file(GLOB LUA_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

# We compile Lua C sources with C++ compiler so we don't have to bother
# with defining 2 different compilers.
# Apply property ONLY to source files, NOT headers.
set_source_files_properties(${LUA_SOURCES} PROPERTIES LANGUAGE CXX)

# Add both sources and headers to the library (headers for IDE visibility)
add_library(liblua ${LUA_SOURCES} ${LUA_HEADERS})
target_include_directories(liblua SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(liblua PROPERTIES LINKER_LANGUAGE CXX)

# Silence all errors. It's C code being compiled as C++, there're lots of them.
target_compile_options(liblua PRIVATE -w)
