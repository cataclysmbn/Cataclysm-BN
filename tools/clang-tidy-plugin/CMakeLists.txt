cmake_minimum_required(VERSION 3.24)
include(ExternalProject)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 23)

set(CataAnalyzerSrc
    CataTidyModule.cpp
    CombineLocalsIntoPointCheck.cpp
    DeterminismCheck.cpp
    JsonTranslationInputCheck.cpp
    NoLongCheck.cpp
    NoStaticGettextCheck.cpp
    PointInitializationCheck.cpp
    SimplifyPointConstructorsCheck.cpp
    StringLiteralIterator.cpp
    TestFilenameCheck.cpp
    TranslatorCommentsCheck.cpp
    UnusedStaticsCheck.cpp
    UseLocalizedSortingCheck.cpp
    UseNamedPointConstantsCheck.cpp
    UsePointApisCheck.cpp
    UsePointArithmeticCheck.cpp
    Utils.cpp
    XYCheck.cpp)

set(CataAnalyzerName CataAnalyzerPlugin)
add_library(${CataAnalyzerName} MODULE ${CataAnalyzerSrc})

target_include_directories(${CataAnalyzerName} SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})

if (NOT "${CATA_CLANG_TIDY_INCLUDE_DIR}" STREQUAL "")
    target_include_directories(${CataAnalyzerName} SYSTEM PRIVATE ${CATA_CLANG_TIDY_INCLUDE_DIR})
endif ()

separate_arguments(LLVM_DEFINITIONS_SEP UNIX_COMMAND "${LLVM_DEFINITIONS}")
target_compile_definitions(${CataAnalyzerName} PRIVATE ${LLVM_DEFINITIONS_SEP})

# We need to turn off exceptions and RTTI to match the LLVM build.
# I feel there ought to be a way to extract these flags from the
# LLVMConfig.cmake as we have done for e.g. LLVM_INCLUDE_DIRS above, but I
# haven't found one.
if (MSVC)

else ()
    target_compile_options(${CataAnalyzerName} PRIVATE -fno-exceptions -fno-rtti)
endif ()

configure_file(test/lit.site.cfg.in test/lit.site.cfg @ONLY)
configure_file(test/.clang-tidy test/.clang-tidy COPYONLY)
