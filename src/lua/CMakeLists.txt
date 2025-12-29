file(GLOB LUA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
file(GLOB LUA_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
list(APPEND LUA_SOURCES ${LUA_HEADERS})

# We compile Lua C sources with C++ compiler so we don't have to bother 
# with defining 2 different compilers.
set_source_files_properties(${LUA_SOURCES} PROPERTIES LANGUAGE CXX)

add_library(liblua ${LUA_SOURCES})
target_include_directories(liblua SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(liblua PROPERTIES LINKER_LANGUAGE CXX)

# Silence all errors. It's C code being compiled as C++, there're lots of them.
target_compile_options(liblua PRIVATE -w)
