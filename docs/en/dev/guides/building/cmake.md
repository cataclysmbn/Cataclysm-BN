---
title: CMake
---

## Prerequisites

You'll need to have these libraries and their development headers installed in order to build
CataclysmBN:

- General
  - `cmake` >= 3.0.0
  - `gcc` >= 14
  - `clang` >= 19
  - `gcc-libs`
  - `glibc`
  - `zlib`
  - `bzip2`
  - `sqlite3`
- Curses
  - `ncurses`
- Tiles
  - `SDL` >= 2.0.0
  - `SDL_image` >= 2.0.0 (with PNG and JPEG support)
  - `SDL_mixer` >= 2.0.0 (with Ogg Vorbis support)
  - `SDL_ttf` >= 2.0.0
  - `freetype`
- Sound
  - `vorbis`
  - `libbz2`
  - `libz`

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

- For Ubuntu-based distros (24.04 onwards):

```sh
sudo apt install git cmake ninja-build mold g++-14 clang-20 ccache \
libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev \
libfreetype-dev bzip2 zlib1g-dev libvorbis-dev libncurses-dev \
gettext libflac++-dev libsqlite3-dev zlib1g-dev
```

- For Fedora-based distros:

```sh
sudo dnf install git cmake ninja-build mold clang ccache \
SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel \
freetype glibc bzip2 zlib-ng libvorbis ncurses gettext flac-devel \
sqlite-devel zlib-devel
```

#### Verifying Compiler Version

You need to have at least `gcc` 14 **and** `clang` 19 to build CataclysmBN. You can check your compiler version with:

```sh
$ g++ --version
g++ (GCC) 15.2.1 20250808 (Red Hat 15.2.1-1)
Copyright (C) 2025 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

$ clang++ --version
clang version 20.1.8 (Fedora 20.1.8-4.fc42)
Target: x86_64-redhat-linux-gnu
Thread model: posix
InstalledDir: /usr/bin
Configuration file: /etc/clang/x86_64-redhat-linux-gnu-clang++.cfg
```

> [!TIP]
>
> **when intalled `gcc-{version}` but `gcc` is not found**
>
> Use `update-alternatives` to set the default gcc version:
>
> ```sh
> sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100
> sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100
> sudo update-alternatives --display gcc
> gcc - auto mode
>   link best version is /usr/bin/gcc-14
>   link currently points to /usr/bin/gcc-14
>   link gcc is /usr/bin/gcc
> /usr/bin/gcc-14 - priority 100
> sudo update-alternatives --display g++
> g++ - auto mode
>   link best version is /usr/bin/g++-14
>   link currently points to /usr/bin/g++-14
>   link g++ is /usr/bin/g++
> /usr/bin/g++-14 - priority 100
> ```
>
> The same applies to `clang`.
>
> ```sh
> sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100
> sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 100
> ```

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
actual build is done using either `make` (for Makefiles generator) or build-system agnostic
`cmake --build .` .

There are two ways to build CataclysmBN with CMake: inside the source tree or outside of it.
Out-of-source builds have the advantage that you can have multiple builds with different options
from one source directory.

> [!CAUTION]
>
> Inside the source tree build is **NOT** supported.

#### Build with Presets (Recommended)

There's multiple predefined [build presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) available, which simplifies build process to just two commands:

```sh
cmake --preset linux-slim
cmake --build build --preset linux-slim --target cataclysm-bn-tiles
```

This will place the executables into `out/build/linux-slim/`.

> [!TIP]
> To build with [clang-tidy plugin](../../reference/tooling.md#clang-tidy) and tracy profiler built-in, try `linux-full`.

> [!NOTE]
> You can build multiple targets at once with:
>
> ```sh
> cmake --build build --preset linux-slim --target cataclysm-bn-tiles cata_test-tiles
> ```
>
> Or limit maximum number of threads with `--parallel` option:
>
> ```sh
> cmake --build build --preset linux-slim --target cataclysm-bn-tiles --parallel 4
> ```

#### Build without Presets

To build CataclysmBN out of source:

```sh
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The above example creates a build directory inside the source directory, but that's not required -
you can just as easily create it in a completely different location.

To install CataclysmBN after building (as root using su or sudo if necessary):

```sh
cmake --install build
```

### Creating Distribution Packages

Use the `dist-tiles` or `dist-curses` presets to create portable distribution packages:

```sh
# Configure for tiles distribution
cmake --preset dist-tiles

# Build the game and tools
cmake --build --preset dist-tiles

# Create distribution package
cmake --install build --prefix cataclysmbn-linux-tiles
```

For curses-only builds:

```sh
cmake --preset dist-curses
cmake --build --preset dist-curses
cmake --install build --prefix cataclysmbn-linux-curses
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

#### Distribution Presets

| Preset        | Description                             |
| ------------- | --------------------------------------- |
| `dist-tiles`  | Tiles + Sound + Languages               |
| `dist-curses` | Curses + Languages                      |
| `lint`        | Minimal build for formatting tools only |

#### Portable vs System Install

| Option          | `USE_PREFIX_DATA_DIR=OFF` | `USE_PREFIX_DATA_DIR=ON`   |
| --------------- | ------------------------- | -------------------------- |
| Data location   | `./data/`                 | `/usr/share/cataclysm-bn/` |
| Config location | `./config/`               | `~/.config/cataclysm-bn/`  |
| Best for        | Portable/release builds   | System packages (deb/rpm)  |

To change build options, you can either pass the options on the command line:

```sh
cmake .. -DOPTION_NAME=option_value
```

Or use either the `ccmake` or `cmake-gui` front-ends, which display all options and their cached
values on a console and graphical UI, respectively.

```sh
ccmake ..
cmake-gui ..
```

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

Make a build directory and run cmake configuration step

```sh
cd <path to cbn sources>
mkdir build
cmake -B build -DTILES=ON -DLANGUAGES=none -DBACKTRACE=OFF -DSOUND=ON
```

Build!

```
cmake --build build -j 2 -- /p:Configuration=Release
```

The `-j 2` flag controls build parallelism - you can omit it if you wish. The
`/p:Configuration=Release` flag is passed directly to MSBuild and controls optimizations. If you
omit it, the `Debug` configuration would be built instead. For powershell you'll need to have an
extra `--` after the first one.

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

A full list of options supported by CMake, you may either run the `ccmake` or `cmake-gui`
front-ends, or run `cmake` and open the generated CMakeCache.txt from the build directory in a text
editor.

```
cmake -DOPTION_NAME1=option_value1 [-DOPTION_NAME2=option_value2 [...]]
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

- CURSES=`<boolean>`

Build curses version.

- TILES=`<boolean>`

Build graphical tileset version.

- SOUND=`<boolean>`

Support for in-game sounds & music.

- USE_HOME_DIR=`<boolean>`

Use user's home directory for save files.

- LANGUAGES=`<str>`

Compile localization files for specified languages. Example:

```
-DLANGUAGES="cs;de;el;es_AR;es_ES"
```

Note that language files are only compiled automatically when building the `RELEASE` build type. For
other build types, you need to add the `translations_compile` target to the `make` command, for
example `make all translations_compile`.

### Building with Translations Locally

Starting from 2026, translation files (`.po` files) are no longer stored in the repository. They are pulled from Transifex during CI builds and packaged as artifacts. This reduces repository size and ensures translations are always up-to-date in releases.

For local development builds, you have two options:

#### Option 1: Build Without Translations (Fastest)

Skip translation compilation entirely:

```sh
cmake --preset linux-full -DLANGUAGES=none
cmake --build --preset linux-full
```

#### Option 2: Pull Translations from Transifex

If you need to test translations locally:

1. Install Transifex CLI:

```sh
curl -sL https://github.com/transifex/cli/releases/download/v1.6.17/tx-linux-amd64.tar.gz | sudo tar zxvf - -C /usr/bin tx
```

2. Pull translations (requires Transifex access):

```sh
tx pull --force
```

3. Build with translations:

```sh
cmake --preset linux-full -DLANGUAGES=all
cmake --build --preset linux-full
```

#### Option 3: Download Pre-built Translations from CI

Download translation artifacts from a recent release or nightly build:

1. Go to [Releases](https://github.com/cataclysmbn/Cataclysm-BN/releases)
2. Extract `lang/po` and `lang/mo` directories from the release archive
3. Place them in your local repository
4. Build normally with `-DLANGUAGES=all`

> [!NOTE]
> For contributors: You don't need translations for code development. Use `-DLANGUAGES=none` to speed up builds.
> Translation updates happen automatically in CI and are included in release builds.

- DYNAMIC_LINKING=`<boolean>`

Use dynamic linking. Or use static to remove MinGW dependency instead.

- CUSTOM LINKER=`<str>`

Choose custom linkers such as [gold], [lld] or [mold].

- Choose ld if you don't use libbacktrace.
- Choose mold if use libbacktrace. It's the fastest linker, outperforming gold by 24x.

[gold]: https://en.wikipedia.org/wiki/Gold_(linker)
[lld]: https://lld.llvm.org
[mold]: https://github.com/rui314/mold

- BACKTRACE=`<boolean>`

On crash, print a backtrace to the console. Defaults to `ON` for debug builds.

- LIBBACKTRACE=`<boolean>`

Print backtrace with [libbacktrace]. This allows lld and mold to print backtrace, and is generally
much faster.

[libbacktrace]: https://github.com/ianlancetaylor/libbacktrace

- USE_TRACY=`<boolean>`

Use tracy profiler. See [Profiling with tracy](../tracy.md) for more information.

- GIT_BINARY=`<str>`

Override default Git binary name or path.

- USE_PREFIX_DATA_DIR=`<boolean>`

Use UNIX system directories for game data in release build.

- USE_XDG_DIR=`<boolean>`

Use XDG directories for save and config files.

- TESTS=`<boolean>`

Whether to build tests.

- JSON_FORMAT=`<boolean>`

Build the `json_formatter` tool and enable `style-json` / `style-json-parallel` targets for
formatting JSON files. See [Formatting & Linting](../formatting.md) for usage.

So a CMake command for building Cataclysm-BN in release mode with tiles and sound support will look
as follows, provided it is run in build directory located in the project.

```sh
cmake ../ -DCMAKE_BUILD_TYPE=Release -DTILES=ON -DSOUND=ON
```
