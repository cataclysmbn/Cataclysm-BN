---
title: Makefile
---

> [!CAUTION]
>
> makefile build is deprecated and will not recieve updates. Please build with [CMake](./cmake.md) instead.

To build Cataclysm from source you will need at least a C++ compiler, some basic developer tools,
and necessary build dependencies. The exact package names vary greatly from distro to distro, so
this part of the guide is intended to give you higher-level understanding of the process.

## Compiler

You have three major choices here: GCC, Clang and MXE.

- GCC is almost always the default on Linux systems so it's likely you already have it
- Clang is usually faster than GCC, so it's worth installing if you plan to keep up with the latest
  nightlies.
- MXE is a cross-compiler, so of any importance only if you plan to compile for Windows on your
  Linux machine

(Note that your distro may have separate packages e.g. `gcc` only includes the C compiler and for
C++ you'll need to install `g++`.)

Cataclysm is targeting C++23 standard and that means you'll need a compiler that supports it. You
can easily check if your version of `g++` supports C++23 by running:

```sh
$ g++ --std=c++23
g++: fatal error: no input files
compilation terminated.
```

If you get a line like:

```sh
g++: error: unrecognized command line option ‘--std=c++23’
```

This means you'll need a newer version of GCC (`g++`).

The general rule is the newer the compiler the better.

## Tools

Most distros seem to package essential build tools as either a single package (Debian and
derivatives have `build-essential`) or a package group (Arch has `base-devel`). You should use the
above if available. Otherwise you'll at least need `make` and figure out the missing dependencies as
you go (if any).

Besides the essentials you will need `git`.

If you plan on keeping up with nightlies you should also install `ccache`, which will considerably
speed-up partial builds.

## Dependencies

There are some general dependencies, optional dependencies and then specific dependencies for either
curses or tiles builds. The exact package names again depend on the distro you're using, and whether
your distro packages libraries and their development files separately (e.g. Debian and derivatives).

Rough list based on building on Arch:

- General: `gcc-libs`, `glibc`, `zlib`, `bzip2`, `sqlite3`
- Optional: `intltool`
- Curses: `ncurses`
- Tiles: `sdl2`, `sdl2_image`, `sdl2_ttf`, `sdl2_mixer`, `freetype2`

E.g. for curses build on Debian and derivatives you'll also need `libncurses5-dev` or
`libncursesw5-dev`.

Note on optional dependencies:

- `intltool` - for building localization files; if you plan to only use English you can skip it

You should be able to figure out what you are missing by reading the compilation errors and/or the
output of `ldd` for compiled binaries.

## Make flags

Given you're building from source you have a number of choices to make:

- `NATIVE=` - you should only care about this if you're cross-compiling
- `RELEASE=1` - without this you'll get a debug build (see note below)
- `LTO=1` - enables link-time optimization with GCC/Clang
- `TILES=1` - with this you'll get the tiles version, without it the curses version
- `SOUND=1` - if you want sound; this requires `TILES=1`
- `LANGUAGES=` - specifies localizations. See details [here](#compiling-localization-files)
- `CLANG=1` - use Clang instead of GCC
- `CCACHE=1` - use ccache
- `USE_LIBCXX=1` - use libc++ instead of libstdc++ with Clang (default on OS X)
- `BACKTRACE=1` - Support for printing stack backtraces on crash

There is a couple of other possible options - feel free to read the `Makefile`.

If you have a multi-core computer you'd probably want to add `-jX` to the options, where `X` should
roughly be twice the number of cores you have available.

Example: `make -j4 CLANG=1 CCACHE=1 NATIVE=linux64 RELEASE=1 TILES=1`

The above will build a tiles release explicitly for 64 bit Linux, using Clang and ccache and 4
parallel processes.

Example: `make -j2`

The above will build a debug-enabled curses version for the architecture you are using, using GCC
and 2 parallel processes.

**Note on debug**: You should probably always build with `RELEASE=1` unless you experience segfaults
and are willing to provide stack traces.

## Compiling localization files

By default, only English language is available, and it does not require localization file.

If you want to compile files for specific languages, you should add
`LANGUAGES="<lang_id_1> [lang_id_2] [...]"` option to make command:

```sh
make LANGUAGES="zh_CN zh_TW"
```

You can get the language ID from the filenames of `*.po` in `lang/po` directory or use
`LANGUAGES="all"` to compile all available localizations.

# Debian

Instructions for compiling on a Debian-based system. The package names here are valid for Ubuntu
12.10 and may or may not work on your system.

Building instructions, below, always assume you are running them from the Cataclysm:BN source
directory.

## Linux (native) ncurses builds

Dependencies:

- ncurses or ncursesw (for multi-byte locales)
- build essentials

Install:

```sh
sudo apt-get install libncurses5-dev libncursesw5-dev build-essential astyle libsqlite3-dev zlib1g-dev
```

### Building

Run:

```sh
make
```

## Linux (native) SDL builds

Dependencies:

- SDL
- SDL_ttf
- freetype
- build essentials
- libsdl2-mixer-dev - Used if compiling with sound support.

Install:

```sh
sudo apt-get install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev libfreetype6-dev build-essential
```

check correct version of SDL2 is installed by running:

```sh
> sdl2-config --version
2.0.22
```

using old version of SDL could result in
[IME not working.](https://github.com/cataclysmbn/Cataclysm-BN/issues/1497)

### Building

A simple installation could be done by simply running:

```sh
make TILES=1
```

A more comprehensive alternative is:

```sh
make -j2 TILES=1 SOUND=1 RELEASE=1 USE_HOME_DIR=1
```

The -j2 flag means it will compile with two parallel processes. It can be omitted or changed to -j4
in a more modern processor. If there is no desire to have sound, those flags can also be omitted.
The USE_HOME_DIR flag places the user files, like configurations and saves into the home folder,
making It easier for backups, and can also be omitted.

## Cross-compile to linux 32-bit from linux 64-bit

Dependencies:

- 32-bit toolchain
- 32-bit ncursesw (compatible with both multi-byte and 8-bit locales)

Install:

```sh
sudo apt-get install libc6-dev-i386 lib32stdc++-dev g++-multilib lib32ncursesw5-dev
```

### Building

Run:

```sh
make NATIVE=linux32
```

## Cross-compile to Windows from Linux

To cross-compile to Windows from Linux, you will need [MXE](http://mxe.cc).

These instructions were written for Ubuntu 20.04, but should be applicable to any Debian-based
environment. Please adjust all package manager instructions to match your environment.

MXE can be either installed from MXE apt repository (much faster) or compiled from source.

### Installing MXE from binary distribution

```sh
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 86B72ED9
sudo add-apt-repository "deb [arch=amd64] https://pkg.mxe.cc/repos/apt `lsb_release -sc` main"
sudo apt-get update
sudo apt-get install astyle bzip2 git make mxe-x86-64-w64-mingw32.static-{sdl2,sdl2-ttf,sdl2-image,sdl2-mixer}
```

Edit your `~/.profile` as follows:

```sh
export PLATFORM_64="/usr/lib/mxe/usr/bin/x86_64-w64-mingw32.static-"
```

This is to ensure that the variables for the `make` command will not get reset after a power cycle.

### Installing MXE from source

Install [MXE requirements](http://mxe.cc/#requirements) and build dependencies:

```sh
sudo apt install astyle autoconf automake autopoint bash bison bzip2 cmake flex gettext git g++ gperf intltool libffi-dev libgdk-pixbuf2.0-dev libtool libltdl-dev libssl-dev libxml-parser-perl lzip make mingw-w64 openssl p7zip-full patch perl pkg-config python ruby scons sed unzip wget xz-utils g++-multilib libc6-dev-i386 libtool-bin
```

Clone MXE repo and build packages required for CBN:

```sh
mkdir -p ~/src
cd ~/src
git clone https://github.com/mxe/mxe.git
cd mxe
make -j$((`nproc`+0)) MXE_TARGETS='x86_64-w64-mingw32.static' sdl2 sdl2_ttf sdl2_image sdl2_mixer sqlite
```

Building all these packages from MXE might take a while, even on a fast computer. Be patient; the
`-j` flag will take advantage of all your processor cores.

If you are not planning on building for both 32-bit and 64-bit, you might want to adjust your
MXE_TARGETS.

Edit your `~/.profile` as follows:

```sh
export PLATFORM_64="~/src/mxe/usr/bin/x86_64-w64-mingw32.static-"
```

This is to ensure that the variables for the `make` command will not get reset after a power cycle.

### Building (SDL)

```sh
make -j$((`nproc`+0)) CROSS="${PLATFORM_64}" TILES=1 SOUND=1 RELEASE=1 BACKTRACE=0 PCH=0 bindist
```

## Cross-compile to Mac OS X from Linux

The procedure is very much similar to cross-compilation to Windows from Linux. Tested on ubuntu
14.04 LTS but should work on other distros as well.

Please note that due to historical difficulties with cross-compilation errors, run-time
optimizations are disabled for cross-compilation to Mac OS X targets. (`-O0` is specified as a
compilation flag.) See
[Pull Request #26564](https://github.com/cataclysmbn/Cataclysm-BN/pull/26564) for details.

### Dependencies

- OSX cross-compiling toolchain [osxcross](https://github.com/tpoechtrager/osxcross)

- `genisoimage` and [libdmg-hfsplus](https://github.com/planetbeing/libdmg-hfsplus.git) to create
  dmg distributions

Make sure that all dependency tools are in search `PATH` before compiling.

### Setup

To set up the compiling environment execute the following commands

```
git clone https://github.com/tpoechtrager/osxcross.git #clone the toolchain
cd osxcross
cp ~/MacOSX10.11.sdk.tar.bz2 ./tarballs/ # copy prepared MacOSX SDK tarball on place.
OSX_VERSION_MIN=11 ./build.sh # build everything
```

[Read more about it](https://github.com/tpoechtrager/osxcross/blob/master/README.md#packaging-the-sdk)
Note the targeted minimum supported version of OSX.

Have a prepackaged set of libs and frameworks in place, since compiling with `osxcross` built-in
MacPorts is rather difficult and not supported at the moment. Your directory tree should look like:

```
~/
├── Frameworks
│   ├── SDL2.framework
│   ├── SDL2_image.framework
│   ├── SDL2_mixer.framework
│   └── SDL2_ttf.framework
└── libs
    └── ncurses
        ├── include
        └── lib
```

Populated with respective frameworks, dylibs and headers. Tested with lib version
libncurses.5.4.dylib for ncurses. These libs were obtained from `homebrew` binary distribution at OS
X 10.11 Frameworks were obtained from SDL official website as described in the next [section](#sdl)

### Building (SDL)

To build full feature tiles and sound enabled version with localizations enabled:

```sh
make dmgdist CROSS=x86_64-apple-darwin15- NATIVE=osx USE_HOME_DIR=1 CLANG=1
  RELEASE=1 LANGUAGES=all TILES=1 SOUND=1 FRAMEWORK=1
  OSXCROSS=1 LIBSDIR=../libs FRAMEWORKSDIR=../Frameworks
```

Make sure that `x86_64-apple-darwin15-clang++` is in `PATH` environment variable.

### Building (ncurses)

To build full curses version with localizations enabled:

```sh
make dmgdist CROSS=x86_64-apple-darwin15- NATIVE=osx USE_HOME_DIR=1 CLANG=1
  RELEASE=1 LANGUAGES=all OSXCROSS=1 LIBSDIR=../libs FRAMEWORKSDIR=../Frameworks
```

Make sure that `x86_64-apple-darwin15-clang++` is in `PATH` environment variable.

## Cross-compile to Android from Linux

The Android build uses [Gradle](https://gradle.org/) to compile the java and native C++ code, and is
based heavily off SDL's
[Android project template](https://github.com/libsdl-org/SDL/tree/main/android-project). See the
official SDL documentation
[README-android.md](https://github.com/libsdl-org/SDL/blob/main/docs/README-android.md) for further
information.

The Gradle project lives in the repository under `android/`. You can build it via the command line
or open it in [Android Studio](https://developer.android.com/studio/). For simplicity, it only
builds the SDL version with all features enabled, including tiles, sound and localization.

### Dependencies

- Java JDK 11
- SDL2 (tested with 2.0.8, though a custom fork is recommended with project-specific bugfixes)
- SDL2_ttf (tested with 2.0.14)
- SDL2_mixer (tested with 2.0.2)
- SDL2_image (tested with 2.0.3)

The Gradle build process automatically installs dependencies from
[deps.zip](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/android/app/deps.zip).

### Setup

Install Linux dependencies. For a desktop Ubuntu installation:

```sh
sudo apt-get install openjdk-8-jdk-headless
```

Install Android SDK and NDK:

```sh
wget https://dl.google.com/android/repository/sdk-tools-linux-4333796.zip
unzip sdk-tools-linux-4333796.zip -d ~/android-sdk
rm sdk-tools-linux-4333796.zip
~/android-sdk/tools/bin/sdkmanager --update
~/android-sdk/tools/bin/sdkmanager "tools" "platform-tools" "ndk-bundle"
~/android-sdk/tools/bin/sdkmanager --licenses
```

Export Android environment variables (you can add these to the end of `~/.bashrc`):

```sh
export ANDROID_SDK_ROOT=~/android-sdk
export ANDROID_HOME=~/android-sdk
export ANDROID_NDK_ROOT=~/android-sdk/ndk-bundle
export PATH=$PATH:$ANDROID_SDK_ROOT/platform-tools
export PATH=$PATH:$ANDROID_SDK_ROOT/tools
export PATH=$PATH:$ANDROID_NDK_ROOT
```

You can also use this additional variables if you want to use `ccache` to speed up subsequnt builds:

```sh
export USE_CCACHE=1
export NDK_CCACHE=/usr/local/bin/ccache
```

**Note:** Path to `ccache` can be different on your system.

### Android device setup

Enable
[Developer options on your Android device](https://developer.android.com/studio/debug/dev-options).
Connect your device to your PC via USB cable and run:

```sh
adb devices
adb connect <devicename>
```

### Building

To build an APK, use the Gradle wrapper command line tool (gradlew). The Android Studio
documentation provides a good summary of how to
[build your app from the command line](https://developer.android.com/studio/build/building-cmdline).

To build a debug APK, from the `android/` subfolder of the repository run:

```sh
./gradlew assembleDebug
```

This creates a debug APK in `./android/app/build/outputs/apk/` ready to be installed on your device.

To build a debug APK and immediately deploy to your connected device over adb run:

```sh
./gradlew installDebug
```

To build a signed release APK (ie. one that can be installed on a device),
[build an unsigned release APK and sign it manually](https://developer.android.com/studio/publish/app-signing#signing-manually).

### Triggering a Nightly Build in a Github Fork

To successfully build an Android APK using a nightly build in your own Github fork, you will need to
initialize a set of dummy Android signing keys. This is necessary because the Github Actions
workflow requires a set of keys to sign the APKs with.

1. Make up a >6 character password. Remember it and save it into github secrets as
   `KEYSTORE_PASSWORD`
2. Create a key via
   `keytool -genkey -v -keystore release.keystore -keyalg RSA -keysize 2048 -validity 10000 -alias dummy-key`.
   When asked for a password, use the password from above.
3. Create a file called `keystore.properties.asc` with the following contents:

```text
storeFile=release.keystore
storePassword=<INSERT PASSWORD FROM STEP 1>
keyAlias=dummy-key
keyPassword=<INSERT PASSWORD FROM STEP 1>
```

4. Encrypt `release.keystore` using the password from step 1 using
   `gpg --symmetric --cipher-algo AES256 --armor release.keystore`. Save the result into github
   secrets as `KEYSTORE`
5. Encrypt `keystore.properties` using the password from step 1 using
   `gpg --symmetric --cipher-algo AES256 --armor keystore.properties`. Save the result into github
   secrets as `KEYSTORE_PROPERTIES`

### Additional notes

The app stores data files on the device in
`/sdcard/Android/data/com.cleverraven/cataclysmdda/files`. The data is backwards compatible with the
desktop version.

## Advanced info for Developers

For most people, the simple Homebrew installation is enough. For developers, here are some more
technical details on building Cataclysm on Mac OS X.

### SDL

SDL2, SDL2_image, and SDL2_ttf are needed for the tiles build. Optionally, you can add SDL2_mixer
for sound support. Cataclysm can be built using either the SDL framework, or shared libraries built
from source.

The SDL framework files can be downloaded here:

- [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.28.3)
- [SDL2_image](https://github.com/libsdl-org/SDL_image)
- [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf)

Copy `SDL2.framework`, `SDL2_image.framework`, and `SDL2_ttf.framework` to `/Library/Frameworks` or
`/Users/name/Library/Frameworks`.

If you want sound support, you will need an additional SDL framework:

- [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer) (optional, for sound support)

Copy `SDL2_mixer.framework` to `/Library/Frameworks` or `/Users/name/Library/Frameworks`.

Alternatively, SDL shared libraries can be installed using a package manager:

For Homebrew:

```sh
brew install sdl2 sdl2_image sdl2_ttf
```

with sound:

```sh
brew install sdl2_mixer libvorbis libogg
```

For MacPorts:

```sh
sudo port install libsdl2 libsdl2_image libsdl2_ttf
```

with sound:

```sh
sudo port install libsdl2_mixer libvorbis libogg
```

### ncurses

ncurses with wide character support enabled is needed since Cataclysm makes extensive use of Unicode
characters

For Homebrew:

```sh
brew install ncurses
```

For MacPorts:

```sh
sudo port install ncurses
hash -r
```

### gcc

The version of gcc/g++ installed with the
[Command Line Tools for Xcode](https://developer.apple.com/downloads/) is actually just a front end
for the same Apple LLVM as clang. This doesn't necessarily cause issues, but this version of gcc/g++
will have clang error messages and essentially produce the same results as if using clang. To
compile with the "real" gcc/g++, install it with homebrew:

```sh
brew install gcc
```

However, homebrew installs gcc as gcc-{version} (where {version} is the version) to avoid conflicts.
The simplest way to use the homebrew version at `/usr/local/bin/gcc-{version}` instead of the Apple
LLVM version at `/usr/bin/gcc` is to symlink the necessary.

```sh
cd /usr/local/bin
ln -s gcc-12 gcc
ln -s g++-12 g++
ln -s c++-12 c++
```

Or, to do this for everything in `/usr/local/bin/` ending with `-12`,

```sh
find /usr/local/bin -name "*-12" -exec sh -c 'ln -s "$1" $(echo "$1" | sed "s/..$//")' _ {} \;
```

Also, you need to make sure that `/usr/local/bin` appears before `/usr/bin` in your `$PATH`, or else
this will not work.

Check that `gcc -v` shows the homebrew version you installed.

### brew clang

If you want to use normal clang instead of apple clang, you can install it with Homebrew:

```sh
brew install llvm
```

Then you can specify the compiler with `COMPILER=$(brew --prefix llvm)/bin/clang++` in your make
command.

It's always good to check that the installed compiler is the one you want.

```sh
$(brew --prefix llvm)/bin/clang++ --version
```

### Compiling

The Cataclysm source is compiled using `make`.

### Make options

- `NATIVE=osx` build for OS X. Required for all Mac builds.
- `OSX_MIN=version` sets `-mmacosx-version-min=`; default is 11.
- `TILES=1` build the SDL version with graphical tiles (and graphical ASCII); omit to build with
  `ncurses`.
- `SOUND=1` - if you want sound; this requires `TILES=1` and the additional dependencies mentioned
  above.
- `FRAMEWORK=1` (tiles only) link to SDL libraries under the OS X Frameworks folders; omit to use
  SDL shared libraries from Homebrew or Macports.
- `LANGUAGES="<lang_id_1>[lang_id_2][...]"` compile localization files for specified languages. e.g.
  `LANGUAGES="zh_CN zh_TW"`. You can also use `LANGUAGES=all` to compile all localization files.
- `RELEASE=1` build an optimized release version; omit for debug build.
- `CLANG=1` build with [Clang](http://clang.llvm.org/), the compiler that's included with the latest
  Command Line Tools for Xcode; omit to build using gcc/g++.
- `MACPORTS=1` build against dependencies installed via Macports, currently only `ncurses`.
- `USE_HOME_DIR=1` places user files (config, saves, graveyard, etc) in the user's home directory.
  For curses builds, this is `/Users/<user>/.cataclysm-bn`, for SDL builds it is
  `/Users/<user>/Library/Application Support/Cataclysm`.
- `DEBUG_SYMBOLS=1` retains debug symbols when building an optimized release binary, making it easy
  for developers to spot the crash site.

In addition to the options above, there is an `app` make target which will package the tiles build
into `Cataclysm.app`, a complete tiles build in a Mac application that can run without Terminal.

For more info, see the comments in the `Makefile`.

### Make examples

Build a release SDL version using Clang:

```sh
make NATIVE=osx RELEASE=1 TILES=1 CLANG=1
```

Build a release SDL version using Clang, link to libraries in the OS X Frameworks folders, build all
language files, and package it into `Cataclysm.app`:

```sh
make app NATIVE=osx RELEASE=1 TILES=1 FRAMEWORK=1 LANGUAGES=all CLANG=1
```

Build a release curses version with curses supplied by Macports:

```sh
make NATIVE=osx RELEASE=1 MACPORTS=1 CLANG=1
```

### Running

For curses builds:

```sh
./cataclysm
```

For SDL:

```sh
./cataclysm-bn-tiles
```

For `app` builds, launch Cataclysm.app from Finder.

### Test suite

The build will also generate a test executable at tests/cata_test. Invoke it as you would any other
executable and it will run the full suite of tests. Pass the `--help` flag to list options.

### dmg distribution

You can build a nice dmg distribution file with the `dmgdist` target. You will need a tool called
[dmgbuild](https://pypi.python.org/pypi/dmgbuild). To install this tool, you will need Python first.
If you are on Mac OS X >= 10.8, Python 2.7 is pre-installed with the OS. If you are on an older
version of OS X, you can download Python
[on their official website](https://www.python.org/downloads/) or install it with homebrew
`brew install python`. Once you have Python, you should be able to install `dmgbuild` by running:

```sh
# This install pip. It might not be required if it is already installed.
curl --silent --show-error --retry 5 https://bootstrap.pypa.io/get-pip.py | sudo python
# dmgbuild install
sudo pip install dmgbuild pyobjc-framework-Quartz
```

Once `dmgbuild` is installed, you will be able to use the `dmgdist` target like this. The use of
`USE_HOME_DIR=1` is important here because it will allow for an easy upgrade of the game while
keeping the user config and his saves in his home directory.

    make dmgdist NATIVE=osx RELEASE=1 TILES=1 FRAMEWORK=1 CLANG=1 USE_HOME_DIR=1

You should see a `Cataclysm.dmg` file.

## Mac OS X Troubleshooting

### ISSUE: Colors don't show up correctly

Open Terminal's preferences, turn on `Use bright colors for bold text` in
`Preferences -> Settings -> Text`

# Windows

See [COMPILING-VS-VCPKG.md](./vs_vcpkg.md) for instructions on how to set up and use a build
environment using Visual Studio on windows.

This is probably the easiest solution for someone used to working with Visual Studio and similar
IDEs. -->

## Building with MSYS2

See [COMPILING-MSYS.md](./msys.md) for instructions on how to set up and use a build environment
using MSYS2 on windows.

MSYS2 strikes a balance between a native Windows application and a UNIX-like environment. There's
some command-line tools that our project uses (notably our JSON linter) that are harder to use
without a command-line environment such as what MSYS2 or CYGWIN provide.

## Building with CYGWIN

See [COMPILING-CYGWIN.md](./cygwin.md) for instructions on how to set up and use a build environment
using CYGWIN on windows.

CYGWIN attempts to more fully emulate a POSIX environment, to be "more unix" than MSYS2. It is a
little less modern in some respects, and lacks the convenience of the MSYS2 package manager.

## Building with Clang and MinGW64

Clang by default uses MSVC on Windows, but also supports the MinGW64 library. Simply replace
`CLANG=1` with `"CLANG=clang++ -target x86_64-pc-windows-gnu -pthread"` in your batch script, and
make sure MinGW64 is in your path. You may also need to apply
[a patch](https://sourceforge.net/p/mingw-w64/mailman/message/36386405/) to `float.h` of MinGW64 for
the unit test to compile.

# BSDs

There are reports of CBN building fine on recent OpenBSD and FreeBSD machines (with appropriately
recent compilers), and there is some work being done on making the `Makefile` "just work", however
we're far from that and BSDs support is mostly based on user contributions. Your mileage may vary.
So far essentially all testing has been on amd64, but there is no (known) reason that other
architectures shouldn't work, in principle.

### Building on FreeBSD/amd64 10.1 with the system compiler

FreeBSD uses clang as the default compiler as of 10.0, and combines it with libc++ to provide C++17
support out of the box. You will however need gmake (examples for binary packages):

`pkg install gmake`

Tiles builds will also require SDL2:

`pkg install sdl2 sdl2_image sdl2_mixer sdl2_ttf`

Then you should be able to build with something like this (you can of course set CXXFLAGS and
LDFLAGS in your .profile or something):

```sh
export CXXFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib"
gmake # ncurses builds
gmake TILES=1 # tiles builds
```

The author has not tested tiles builds, as the build VM lacks X; they do at least compile/link
successfully.

### Building ncurses version on FreeBSD/amd64 9.3 with GCC 4.8.4 from ports

For ncurses build add to `Makefile`, before `VERSION`:

```make
OTHERS += -D_GLIBCXX_USE_C99
CXX = g++48
CXXFLAGS += -I/usr/local/lib/gcc48/include
LDFLAGS += -rpath=/usr/local/lib/gcc48
```

Note: or you can `setenv` the above (merging `OTHERS` into `CXXFLAGS`), but you knew that.

And then build with `gmake RELEASE=1`.

### Building on OpenBSD/amd64 5.8 with GCC 4.9.2 from ports/packages

First, install g++, gmake, and libexecinfo from packages (g++ 4.8 or 4.9 should work; 4.9 has been
tested):

`pkg_add g++ gmake libexecinfo`

Then you should be able to build with something like:

`CXX=eg++ gmake`

Only an ncurses build is possible on 5.8-release, as SDL2 is broken. On recent -current or
snapshots, however, you can install the SDL2 packages:

`pkg_add sdl2 sdl2-image sdl2-mixer sdl2-ttf`

and build with:

`CXX=eg++ gmake TILES=1`

### Building on NetBSD/amd64 7.0RC1 with the system compiler

NetBSD has (or will have) gcc 4.8.4 as of version 7.0, which is new enough to build cataclysm. You
will need to install gmake and ncursesw:

`pkgin install gmake ncursesw`

Then you should be able to build with something like this (LDFLAGS for ncurses builds are taken care
of by the ncurses configuration script; you can of course set CXXFLAGS/LDFLAGS in your .profile or
something):

```sh
export CXXFLAGS="-I/usr/pkg/include"
gmake # ncurses builds
LDFLAGS="-L/usr/pkg/lib" gmake TILES=1 # tiles builds
```

SDL builds currently compile, but did not run in my testing - not only do they segfault, but gdb
segfaults when reading the debug symbols! Perhaps your mileage will vary.
