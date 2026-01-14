# Visual Studio + vcpkg

This guide contains steps required to allow compilation of Cataclysm-BN on Windows using Visual
Studio and vcpkg.

Steps from current guide were tested on Windows 10 and 11 (64 bit), Visual Studio 2019 and 2022 (64
bit), but should as well work with slight modifications for other versions of Windows and Visual
Studio.

## Prerequisites:

- Computer with modern Windows operating system installed (Windows 10 or 11), Windows 7 and 8.1 are
  not guaranteed to work;
- NTFS partition with ~15 Gb free space (~10 Gb for Visual Studio, ~1 Gb for vcpkg installation, ~3
  Gb for repository and ~1 Gb for build cache);
- Git for Windows (installer can be downloaded from [Git homepage](https://git-scm.com/));
- Visual Studio 2019 or 2022
  - **Note**: If you are using Visual Studio 2022, you must install the Visual Studio 2019 compilers
    to work around a vcpkg bug. In the Visual Studio Installer, select the 'Individual components'
    tab and search for / select the component that looks like 'MSVC v142 - VS 2019 C++ x64/x86 Build
    Tools'. See https://github.com/microsoft/vcpkg/issues/22287.
- Latest version of vcpkg (see instructions on
  [vcpkg homepage](https://github.com/Microsoft/vcpkg)).
- If you plan on contributing your changes to Bright Nights, you'll also have to install a code
  formatter, see [Code style](#code-style) section for more info.

**Note:** Windows XP is unsupported!

## Installation and configuration:

1. Install `Visual Studio` (installer can be downloaded from
   [Visual Studio homepage](https://visualstudio.microsoft.com/)).

- Select the "Desktop development with C++" and "Game development with C++" workloads.

2. Install `Git for Windows` (installer can be downloaded from
   [Git homepage](https://git-scm.com/)).

3. Install and configure latest `vcpkg`:

_**WARNING: It is important that, wherever you decide to clone this repo, the path does not include
whitespace. That is, `C:/dev/vcpkg` is acceptable, but `C:/dev test/vcpkg` is not.**_

```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat -disableMetrics
vcpkg integrate install
```

## Cloning and compilation:

1. Clone Cataclysm-BN repository with following command line:

**Note:** This will download the entire CBN repository; 3+ GB of data. If you're just testing you
should probably add `--depth=1`.

```cmd
git clone https://github.com/cataclysmbn/Cataclysm-BN.git
cd Cataclysm-BN
```

2. Open the provided solution (`msvc-full-features\Cataclysm-vcpkg-static.sln`) in `Visual Studio`,
   select configuration (`Release` is advised if you just want to compile, `Debug` is if you're
   planning on editing code) and platform (`x64` or `x86`) and build it. All necessary dependencies
   will be built and cached for future use by vcpkg automatically.

3. Open the `Build > Configuration Manager` menu and adjust `Active solution configuration` and
   `Active solution platform` to match your intended target.

This will configure Visual Studio to compile the release version, with support for Sound, Tiles, and
Localization (note, however, that language files themselves are not automatically compiled; this
will be done later).

4. Start the build process by selecting either `Build > Build Solution` or
   `Build > Build > Cataclysm-vcpkg-static`. The process may take a long period of time, so you'd
   better prepare a cup of coffee and some books in front of your computer :) The first build of
   each architecture will also download and install dependencies through vcpkg, which can take an
   especially long time.

5. If you want to launch the game directly from Visual Studio, make sure you have specified correct
   working directory as explained below. Otherwise, you'll only be able to launch the game from the
   file explorer.

6. If you need localization support, execute the bash script `lang/compile_mo.sh` inside Git Bash
   GUI just like on a UNIX-like system. This will compile the language files that were not
   automatically compiled in step 2 above.

### Running from Visual Studio and debugging

1. Ensure that the Cataclysm game binary project (`Cataclysm-vcpkg-static`) is the selected startup
   project (right click on it in Solution Explorer -> `Set as Startup Project`)

2. Configure the working directory in the project properties to `$(ProjectDir)..` (right click on it
   in Solution Explorer -> `Properties` -> select `All Configurations` and `All Platforms` at the
   top -> `Debugging` -> `Working Directory`)

3. Press the debug button (green right-facing triangle near the top, or use the appropriate
   shortcut, e.g. F5)

If you discover that after pressing the debug button in Visual Studio, Cataclysm just exits after
launch with return code 1, that is because of the wrong working directory.

### Debug vs Release builds

`Debug` builds run significantly slower than `Release` builds, but provide additional safety checks.

If you just want to build the executable and play the game, `Release` is advised.

If you plan on editing the code, `Debug` is advised.

If you have enough experience with C++ to know:

- under-the-hood differences between `Debug` and `Release`
- how `Release` optimizations may affect the debugger
- how to avoid undefined behavior in code

Then you might want to use `Release` build all the time to speed up dev process, and disable
optimizations on a file-by-file basis by adding

```cpp
#pragma optimize("", off)
```

line at the top of the file.

### Running unit tests

Ensure that the Cataclysm test binary project (`Cataclysm-test-vcpkg-static`) is the selected
startup project, configure the working directory in the project properties to `$(ProjectDir)..`, and
then press the debug button (or use the appropriate shortcut, e.g. F5). This will run all of the
unit tests. Additional command line arguments may be configured in the project's command line
arguments setting, or if you are using a compatible unit test runner (e.g. Resharper) you can run or
debug individual tests from the unit test sessions.

### Code style

We use `Artistic Style` source code formatter to keep the style of our C++ code consistent. While
it's available as pre-built Windows executables, which you could install and run or configure to
automatically format the code before commit, a much more convenient option for Visual Studio users
is to install a specific extension, see
["Astyle extensions for Visual Studio" in tooling](../../reference/tooling.md#astyle-extensions-for-visual-studio)
for more info.

As of October 2022, the code style check is run automatically on each PR on GitHub, so if you forgot
to style your changes you'll see the corresponsing check failing.

### Make a distribution

There is a batch script in `msvc-full-features` folder `distribute.bat`. It will create a sub folder
`distribution` and copy all required files(eg. `data/`, `Cataclysm.exe` and dlls) into that folder.
Then you can zip it and share the archive on the Internet.
