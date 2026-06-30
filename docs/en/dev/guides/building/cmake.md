---
title: CMake
---

## Prerequisites

You'll need to have these libraries and their development headers installed in order to build
CataclysmBN:

- General
  - `cmake` >= 3.2.4
  - `clang` >= 22
  - `gcc-libs` or equivalent C++ runtime libraries
  - `glibc`
  - `zlib`
  - `bzip2`
  - `sqlite3`
  - `SDL` >= 3.0.0 *
  - `SDL_shadercross` >= 3.0.0 *
- Curses
  - `ncurses`
- Tiles
  - `SDL_image` >= 3.0.0 (with PNG and JPEG support) *
  - `SDL_mixer` >= 3.0.0 (with Ogg Vorbis support) *
  - `SDL_ttf` >= 3.0.0 *
  - `freetype`
- Sound
  - `vorbis`
  - `libbz2`
  - `libz`

> [!NOTE]
> Dependancies marked with * will be built automatically if missing.

In order to compile localization files, you'll also need `gettext` package.

## Build Environment

You can obtain the source code tarball for the latest version from
[git](https://github.com/cataclysmbn/Cataclysm-BN).

```sh
git clone --filter=blob:none https://github.com/cataclysmbn/Cataclysm-BN.git
cd Cataclysm-BN
```

> [!TIP]
> `filter=blob:none` creates a [blobless clone](https://github.blog/open-source/git/get-up-to-speed-with-partial-clone-and-shallow-clone/), which makes the initial clone much faster by downloading files on-demand.

### UNIX Environment

Obtain packages specified above with your system package manager.

- For Ubuntu-based distros (26.04 onwards):

```sh
sudo apt install git cmake ninja-build mold clang-22 llvm-22 ccache \
libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev \
libfreetype-dev bzip2 zlib1g-dev libvorbis-dev libncurses-dev \
gettext libflac++-dev libsqlite3-dev zlib1g-dev
```

- For Fedora-based distros:

```sh
sudo dnf install git cmake ninja-build mold clang llvm ccache \
SDL3-devel SDL3_image-devel SDL3_ttf-devel \
freetype glibc bzip2 zlib-ng libvorbis ncurses gettext flac-devel \
sqlite-devel zlib-devel
```

> [!NOTE]
> Neither Ubuntu or Fedora ship SDL3_mixer or shadercross
> These libraries will be build automatically when compiling

#### Verifying Compiler Version

You need Clang 22 or newer to build CataclysmBN. You can check your compiler version with:

```sh
$ clang++ --version
clang version 22.1.6 (Fedora 22.1.6-1.fc44)
Target: x86_64-redhat-linux-gnu
Thread model: posix
InstalledDir: /usr/bin
```

> [!TIP]
>
> **when installed `clang-{version}` but `clang` is not found**
>
> Use `update-alternatives` to set the default Clang version:
>
> ```sh
> sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-22 100
> sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-22 100
> ```
>
> If Ubuntu only installs versioned LLVM binutils such as `llvm-ar-22` and
> `llvm-ranlib-22`, register those names too:
>
> ```sh
> sudo update-alternatives --install /usr/bin/llvm-ar llvm-ar /usr/bin/llvm-ar-22 100
> sudo update-alternatives --install /usr/bin/llvm-ranlib llvm-ranlib /usr/bin/llvm-ranlib-22 100
> ```

### macOS Environment

Install dependencies via [Homebrew](https://brew.sh/):

```sh
brew install cmake ninja ccache sdl3 sdl3_image sdl3_ttf sdl3_mixer \
  freetype gettext sqlite pkg-config ncurses flac
```

> [!NOTE]
> Apple Clang shipped with Xcode 16+ supports the C++23 features required by CataclysmBN.
> You do **not** need to install a separate compiler.

### Windows Subsystem for Linux (WSL)

Follow the same instructions for `UNIX environment`; it just works (TM)

If you plan on using `tiles`, make sure you have the latest [WSL 2 that supports GUI](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps) and [have installed matching drivers](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps#prerequisites).

### Windows Environment (MSYS2)

1. Follow steps from here: https://msys2.github.io/
2. Install CataclysmBN build deps:

```sh
pacman -S mingw-w64-x86_64-toolchain msys/git \
   	  mingw-w64-x86_64-cmake \
   	  mingw-w64-x86_64-SDL2_{image,mixer,ttf} \
   	  ncurses-devel \
      gettext \
      base-devel
```

This should get your environment set up to build console and tiles version of windows.

> [!NOTE]
>
> If you're trying to test with Jetbrains CLion, point to the cmake version in the `msys32/mingw32`
> path instead of using the built in. This will let cmake detect the installed packages.

### CMake Build

CMake has separate configuration and build steps. Configuration is done using CMake itself, and the
actual build is done using build-system agnostic `cmake --build .`.

There are two ways to build CataclysmBN with CMake: inside the source tree or outside of it.
Out-of-source builds have the advantage that you can have multiple builds with different options
from one source directory.

> [!CAUTION]
>
> Inside the source tree build is **NOT** supported.

#### Build with Presets (Recommended)

There's multiple predefined [build presets](#build-presets) available, which simplifies build process to just two commands.

For Linux:

```sh
cmake --preset linux-slim
cmake --build --preset linux-slim --target cataclysm-bn-tiles
```

For macOS:

```sh
cmake --preset osx-arm-slim
cmake --build --preset osx-arm-slim
```

This will place the executable into `out/build/linux-slim/` or `out/build/osx-arm-slim/` respectively.

> [!TIP]
> To build with [clang-tidy plugin](../../reference/tooling.md#clang-tidy) and tracy profiler support, try `linux-full`.

> [!NOTE]
> You can build multiple targets at once with:
>
> ```sh
> cmake --build --preset linux-slim --target cataclysm-bn-tiles cata_test-tiles
> ```
>
> Or limit maximum number of threads with `--parallel` option:
>
> ```sh
> cmake --build --preset linux-slim --target cataclysm-bn-tiles --parallel 4
> ```

#### Build without Presets

To build CataclysmBN out of source:

```sh
mkdir -p out/build/custom
cmake -B out/build/custom -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/custom
```

The above example creates a build directory under `out/build/`, but that's not required - you can
just as easily create it in a completely different location.

To install CataclysmBN after building (as root using su or sudo if necessary):

```sh
cmake --install out/build/custom
```

### Creating Distribution Packages

> [!TIP]
> Check [build presets](#build-presets) for a list of distribution presets.

Use a build preset to create a portable distribution package:

```sh
# Configure for tiles distribution
cmake --preset dist-tiles

# Build the game and tools
cmake --build --preset dist-tiles

# Create distribution package
cmake --install build --prefix cataclysmbn-linux-tiles
```

This creates a self-contained directory with the following structure:

```
cataclysmbn-linux-tiles/
├── cataclysm-bn-tiles     # Game executable
├── cataclysm-launcher     # Launcher script
├── json_formatter         # JSON formatting tool
├── data/                  # Game data files
├── gfx/                   # Tilesets
├── lang/                  # Translations
├── doc/                   # Documentation
├── README.md
├── LICENSE.txt
└── VERSION.txt
```

To create a tarball for distribution:

```sh
tar -czvf cataclysmbn-linux-tiles.tar.gz cataclysmbn-linux-tiles
```

> [!TIP]
> The `cataclysm-launcher` script sets up the correct working directory and library paths.
> Use it to run the game from any location.

#### Portable vs System Install

| Option          | `USE_PREFIX_DATA_DIR=OFF` | `USE_PREFIX_DATA_DIR=ON`   |
| --------------- | ------------------------- | -------------------------- |
| Data location   | `./data/`                 | `/usr/share/cataclysm-bn/` |
| Config location | `./config/`               | `~/.config/cataclysm-bn/`  |
| Best for        | Portable/release builds   | System packages (deb/rpm)  |

> [!TIP]
> Check [build options](#build-options) for information on how to configure builds

## Build for Visual Studio / MSBuild

> [!CAUTION]
>
> This guide is quite old and requires manual dependency management.
>
> For modern alternative, see [CMake Visual Studio build with vcpkg](./vs_cmake.md)

CMake can generate `.sln` and `.vcxproj` files used either by Visual Studio itself or by MSBuild
command line compiler (if you don't want a full fledged IDE) and have more "native" binaries than
what MSYS/Cygwin can provide.

At the moment only a limited combination of options is supported (tiles only, no localizations, no
backtrace).

Get the tools:

- CMake from the official site - <https://cmake.org/download/>.
- Microsoft compiler - <https://visualstudio.microsoft.com/downloads/?q=build+tools> , choose "Build
  Tools for Visual Studio 2017". When installing chose "Visual C++ Build Tools" options.
  - alternatively, you can get download and install the complete Visual Studio, but that's not
    required.

Get the required libraries:

- [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.28.3) (you need the "(Visual C++
  32/64-bit)" version. Same below)
- [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf)
- [SDL2_image](https://github.com/libsdl-org/SDL_image)
- [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer) (optional, for sound support)
- Unsupported (and unused in the following instructions) optional libs:
  - `ncurses` - ???

Unpack the archives with the libraries.

Open windows command line (or powershell), set the environment variables to point to the libs above
as follows (adjusting the paths as appropriate):

```sh
set SDL2DIR=C:\path\to\SDL2-devel-2.0.9-VC
set SDL2TTFDIR=C:\path\to\SDL2_ttf-devel-2.0.15-VC
set SDL2IMAGEDIR=C:\path\to\SDL2_image-devel-2.0.4-VC
set SDL2MIXERDIR=C:\path\to\SDL2_mixer-devel-2.0.4-VC
```

(for powershell the syntax is `$env:SDL2DIR="C:\path\to\SDL2-devel-2.0.9-VC"`).

Run the CMake configuration step

```sh
cd <path to cbn sources>
cmake -B out/build/msbuild -DTILES=ON -DLANGUAGES=none -DBACKTRACE=OFF -DSOUND=ON
```

Build!

```
cmake --build out/build/msbuild --config Release --parallel 2
```

The `--parallel 2` flag controls build parallelism - you can omit it if you wish. The
`--config Release` flag selects the optimized Visual Studio configuration. If you omit it, the
`Debug` configuration would be built instead.

The resulting files will be put into a `Release` directory inside your source Cataclysm-BN folder.
To make them run you'd need to first move them to the source Cataclysm-BN directory itself (so that
the binary has access to the game data), and second put the required `.dll`s into the same folder -
you can find those inside the directories for dev libraries under `lib/x86/` or `lib/x64/` (you
likely need the `x86` ones even if you're on 64-bit machine).

The copying of dlls is a one-time task, but you'd need to move the binary out of `Release/` each
time it's built. To automate it a bit, you can configure cmake and set the desired binaries
destination directory with `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=` option (and similar for
`CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG`).

Run the game. Should work.

## Build Options

To change build options, you may either pass the options on the command line:

```sh
cmake -DOPTION_NAME1=option_value1 [-DOPTION_NAME2=option_value2 [...]]
```

Or use either the `ccmake` or `cmake-gui` front-ends, which display all options and their cached
values on a console and graphical UI, respectively.

```sh
cmake --preset linux-slim
ccmake out/build/linux-slim
```

```sh
cmake --preset linux-slim
cmake-gui -S . -B out/build/linux-slim
```

### CMake specific options

- CMAKE_BUILD_TYPE=`<build type>`

Selects a specific build configuration when compiling. `release` produces the default, optimized
(-Os) build for regular use. `debug` produces a slower and larger unoptimized (-O0) build with full
debug symbols, which is often needed for obtaining detailed backtraces when reporting bugs.

**NOTE**: By default, CMake will produce `debug` builds unless a different configuration option is
passed in the command line.

- CMAKE_INSTALL_PREFIX=`<full path>`

Installation prefix for binaries, resources, and documentation files.

### CataclysmBN specific options

| Option                | Default                                 | Effect                                                                            |
| --------------------- | --------------------------------------- | --------------------------------------------------------------------------------- |
| `CURSES`              | `ON`                                    | Build curses version.                                                             |
| `TILES`               | `OFF`                                   | Build graphical tileset version.                                                  |
| `SOUND`               | `ON`                                    | Build audio support.                                                              |
| `LANGUAGES`           | `" "`                                   | Build specificed language support.                                                |
| `TESTS`               | `ON`                                    | Build test units.                                                                 |
| `USE_HOME_DIR`        | `ON` (`OFF` on WIN32)                   | Use $HOME directory for save and config files.                                    |
| `USE_XDG_DIR`         | `OFF`                                   | Use XDG directories for save and config files.                                    |
| `USE_PREFIX_DATA_DIR` | `OFF`                                   | Use UNIX system directories for game data in release build.                       |
| `JSON_FORMAT`         | `OFF`                                   | Build JSON formatter.                                                             |
| `CATA_CCACHE`         | `ON`                                    | Try to find and build with ccache.                                                |
| `BUILD_SDL3`          | `OFF`                                   | Force Build SDL3 instead of using system libraries.                               |
| `BUILD_SHADERCROSS`   | `ON`                                    | Build SDL_shadercross from source when shadercross is not on PATH.                |
| `SHADER_TARGETS`      | `spirv;msl` (`dxil;spirv;msl` on WIN32) | Shaders to build.                                                                 |
| `DYNAMIC_LINKING`     | `ON`                                    | Use dynamic linking. Or use static to remove MinGW dependency instead.            |
| `LINKER`              | `" "`                                   | Custom Linker to use                                                              |
| `BACKTRACE`           | `ON`                                    | Support for printing stack backtraces on crash.                                   |
| `LIBBACKTRACE`        | `OFF`                                   | Print backtrace with libbacktrace.                                                |
| `USE_TRACY`           | `OFF`                                   | Use tracy profiler. See [Profiling with tracy](../tracy.md) for more information. |
| `GIT_BINARY`          | `" "`                                   | Git binary name or path.                                                          |

### Build Presets

Check the [cmake docs](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) for more information

| Preset Name                | Description                                         |
| -------------------------- | --------------------------------------------------- |
| `linux-curses`             | Linux Slim Build (Curses)                           |
| `linux-slim`               | Linux Slim Build (Tiles, Sounds)                    |
| `linux-full`               | Linux Full Build (Tiles, Sounds, Clang-Tidy, Tracy) |
| `linux-cross-aarch64`      |                                                     |
| `linux-slim-cross-aarch64` |                                                     |
| `linux-full-cross-aarch64` |                                                     |
| `dist-tiles`               | Linux Distribution (Tiles, Sounds, Languages)       |
| `dist-curses`              | Linux Distribution (Curses, Languages)              |
| `osx-curses-x64-dist`      | macOS x64 Distribution (Curses, Languages)          |
| `osx-tiles-x64-dist`       | macOS x64 Distribution (Tiles, Sounds, Languages)   |
| `osx-arm-slim`             | macOS ARM Slim Build (Tiles, Sounds)                |
| `osx-curses-arm-dist`      | macOS ARM Distribution (Curses, Languages)          |
| `osx-tiles-arm-dist`       | macOS ARM Distribution (Tiles, Sounds, Languages)   |
| `osx-arm-dist`             | macOS ARM Distribution (Tiles, Sounds, Languages)   |
| `lint`                     | Lint and Format Only                                |
| `ci-curses`                | CI Build (Curses)                                   |
| `ci-tiles`                 | CI Build (Tiles, Sound)                             |

> [!NOTE]
>
> The macOS distribution presets build a DMG with the `dmgdist` target. `dmgbuild` must be available on `PATH`.
> If you do not have dmgdist and biplist installed already, you can install them using pip
>
> ```sh
> python3 -m pip install dmgbuild biplist
> ```

### Building with Translations Locally

Starting in 2026, translation files (`.po` files) are no longer kept in the repository. CI pulls them from Transifex, stores them in workflow artifacts, and uses them to build release packages.

For local builds, use one of the following:

#### Option 1: Build Without Translations (Fastest)

If you are not working on translations, disable them:

```sh
cmake --preset linux-full -DLANGUAGES=none
cmake --build --preset linux-full
```

#### Option 2: Pull Translations from Transifex

If you need to test translations locally and have Transifex access:

1. Install the Transifex CLI:

```sh
curl -sL https://github.com/transifex/cli/releases/download/v1.6.17/tx-linux-amd64.tar.gz | sudo tar zxvf - -C /usr/bin tx
```

2. Pull the translation files:

```sh
tx pull --force --all
```

3. Build with translations enabled:

```sh
cmake --preset linux-full -DLANGUAGES=all
cmake --build --preset linux-full
```

#### Option 3: Download the `translations` Workflow Artifact

If you do not have Transifex access, use the artifact produced by the translation workflow:

1. Open a recent successful workflow run in [Actions](https://github.com/cataclysmbn/Cataclysm-BN/actions)
2. Download the `translations` artifact
3. Extract `lang/po` and `src/lang_stats.inc` into your local checkout
4. Build normally with `-DLANGUAGES=all`

> [!NOTE]
> Most code changes do not need translations. Use `-DLANGUAGES=none` unless you are testing localized output.

> [!NOTE]
> Release archives only include compiled `lang/mo` files for packaged builds. They do not contain the `lang/po` sources required to rebuild translations locally.
