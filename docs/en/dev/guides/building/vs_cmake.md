# CMake + Visual Studio + Vcpkg

> [!CAUTION]
>
> CMake build is work-in-progress.

> [!TIP]
>
> If you prefer a prompt-driven workflow launched from Visual Studio, see
> [Visual Studio External Tool Automation (Windows + WSL)](./vs_external_tool_wsl.md).

## Prerequisites

- `cmake` >= 3.24.0
- `vcpkg` from [vcpkg.io](https://vcpkg.io/en/getting-started.html)

Note that starting from Visual Studio 2022 version 17.6, `vcpkg` is included with the distribution
and is available in the VS developer command prompt, so you don't have to install it separately.

## Configure

Configuration can be done using one of the presets in `CMakePresets.json`. They will all build the
code into the directory `out/build/<preset>/`.

### Terminal

Ensure that `cmake` can find `vcpkg`. If it can't, it will complain about missing packages. You can
do this in one of the following ways:

- For VS2022 users with preinstalled `vcpkg`, `vcpkg` should already be available if you're running
  the VS developer command prompt and not the plain terminal
- Append `-DVCPKG_ROOT=C:\dev\vcpkg` (or whatever the path is) to any cmake configure commands
- Set the environment variable `VCPKG_ROOT` to the path to the vcpkg checkout
- Add the `VCPKG_ROOT` cache variable in `CMakePresets.json` with the appropriate path (not
  recommended if you plan to work with the code later, git tracks this file)

Run the command

```sh
cmake --list-presets
```

It will show the presets available to you. The list changes based on the environment you are in. If
empty, the environment is not supported.

Run the command

```sh
cmake --preset <preset>
```

It will download all dependencies and generate build files, as long as `vcpkg` installation is
available.

If you're using VS2022, be sure to select a preset with `2022` in its name, as presets without that
suffix will target VS2019.

You can override any option by appending `-Doption=value` to this command, see
[Build options](./cmake.md/#build-options) in CMake guide. For example, you can disable building of
tests with `-DTESTS=OFF` if you don't care about them.

### Visual Studio

Open the game source folder in Visual Studio.

Visual Studio should be able to recognize the folder as a CMake project, and may attempt to start
configuring it, which will most likely fail because it didn't use the proper preset.

The Standard toolbar shows the presets in the `Configuration` drop-down box. Choose the proper one
(should contain `windows` and `msvc`), then from the main menu, select `Project` ->
`Configure Cache`.

If you're using VS2022, be sure to select a preset with `2022` in its name, as presets without that
suffix are targeting VS2019.

## Build

### Terminal

Run the command

- `cmake --build --preset <preset> --config Release`

You can replace `Release` with `Debug` to get a debug build, or `RelWithDebInfo` for a release build
with less optimizations but more debug information.

### Visual Studio

From the Standard toolbar's `Build Preset` drop-down menu select the build preset. From the main
menu, select `Build` -> `Build All`.

You can also select between `Release`, `Debug` and `RelWithDebInfo` builds, though depending on UI
layout the drop-down menu for this may be hidden behind the overflow button.

## Translations

Translations are optional and require `msgfmt` binary from `gettext` package; `vcpkg` should install
it automatically.

### Terminal

Run the command

- `cmake --build --preset <preset> --target translations_compile`

### Visual Studio

Visual Studio should have built the translations in the previous step. If it did not, open Solution
Explorer, switch it into CMake Targets mode (can be done with right click), then right click on
`translations_compile` target -> `Build translations_compile`.

## Install

> [!CAUTION]
>
> Install is still considered WIP and has received little testing.

### Visual Studio

From the main menu, select `Build` -> `Install CataclysmBN`

### Terminal

Run the command

- `cmake --install out/build/<preset>/ --config Release`

Replace `Release` with your chosen build type.

## Run

The game and test executables will both be available in `.\Release\` folder (folder name matches
build type, so for other build types you'll get other folder names).

You can run them manually from the terminal, as long as you do it from the project's top directory
as by default the game expects data files to be in current path.

For running and debugging from Visual Studio, it's recommended to open the generated VS solution
located at `out\build\<preset>\CataclysmBN.sln` (it will be there regardless of whether you've
completed previous steps in IDE or terminal) and do any further work with it instead.

Alternatively, it's possible to stay in the "Open Folder" mode, but then you'll have to customize
launch configuration for the game executable (and tests), and there may be other yet undiscovered
side effects.

### Terminal

To start the game, run

- `.\Release\cataclysm-bn-tiles.exe`

To execute tests, run

- `.\Release\cata_test-tiles.exe`

### Visual Studio (Option 1, Recommended)

Close Visual Studio, then navigate to `out\build\<preset>\` and open `CataclysmBN.sln`. Set
`cataclysm-bn-tiles` as Startup Project (can be done with right click from Solution Explorer), and
you'll be able to run and debug the game executable without additional issues. It will already be
preconfigured to look for the data files in the top project directory.

To run tests, switch the Startup Project to `cata_test-tiles`.

### Visual Studio (Option 2)

Due to how Visual Studio handles CMake projects, it's impossible to specify the working directory
for the executable while VS is in the "Open Folder" mode. This StackOverflow answer explains it
nicely: https://stackoverflow.com/a/62309569 Fortunately, VS allows customizing exe launch options
on individual basis.

Open solution explorer and switch it into CMake Targets mode if you haven't already (can be done
with a right click). There, right click on the `cataclysm-bn-tiles` target ->
`Add Debug Configuration`. Visual Studio will open launch configurations file for this project, with
new configuration for the `cataclysm-bn-tiles` target. Add the following line:

```
"currentDir": "${workspaceRoot}",
```

to the config and save the file.

The final result should look something like this:

```json
{
  "version": "0.2.1",
  "defaults": {},
  "configurations": [
    {
      "currentDir": "${workspaceRoot}",
      "type": "default",
      "project": "CMakeLists.txt",
      "projectTarget": "cataclysm-bn-tiles.exe (<PATH_TO_SOURCE_FOLDER>\\Debug\\cataclysm-bn-tiles.exe)",
      "name": "cataclysm-bn-tiles.exe (<PATH_TO_SOURCE_FOLDER>\\Debug\\cataclysm-bn-tiles.exe)"
    }
  ]
}
```

Now, you should be able to run and debug the game executable from inside Visual Studio.

If you'd like to run tests, repeat this process for the `cata_test-tiles` target.
