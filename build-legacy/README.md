# Legacy Build Systems

This directory contains legacy build systems that have been superseded by CMake.

**All platforms now use CMake** as the primary build system. Please refer to the main README and documentation.

## Contents

- `Makefile` - Legacy GNU Make build system (previously used for Linux/macOS/BSD)
- `tests-Makefile` - Legacy test Makefile
- `msvc-full-features/` - Legacy Visual Studio project files

## Migration

All functionality from these legacy build systems has been migrated to CMake:

- Windows builds now use CMake with vcpkg
- Linux builds use CMake with system packages or vcpkg
- macOS builds use CMake with Homebrew dependencies
- Android builds continue to use Gradle (not affected by this migration)

For build instructions, see: [docs/en/dev/guides/building/cmake.md](../docs/en/dev/guides/building/cmake.md)
