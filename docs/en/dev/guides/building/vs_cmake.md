# Building with Visual Studio 2022 and CMake

This guide covers building Cataclysm: Bright Nights on Windows using
Visual Studio 2022's native CMake integration. After the one-time setup,
everything — configuration, building, debugging — is done directly inside
Visual Studio with no external tools required.

> **Legacy build:** The `.sln`-based build in `msvc-full-features/` still works
> and is untouched by this system. Both can coexist in the same checkout.

## How it works

The project ships two CMake configuration files:

| File                 | Used by              |
| -------------------- | -------------------- |
| `CMakeSettings.json` | Visual Studio IDE    |
| `CMakePresets.json`  | cmake CLI, CI, Linux |

VS reads `CMakeSettings.json` directly when you open the folder. No manual
cmake configure step is required before opening VS.

> **VS setting:** In **Tools → Options → CMake**, set
> _"When a CMakeSettings.json or CMakePresets.json file is detected"_ to
> **"Use CMakeSettings.json (Legacy)"** or **"Never use CMake Presets"**.
> This ensures VS uses `CMakeSettings.json` and ignores `CMakePresets.json`.

> [!TIP]
>
> If you prefer a prompt-driven workflow launched from Visual Studio, see
> [Visual Studio External Tool Automation (Windows + WSL)](./vs_external_tool_wsl.md).

## Prerequisites

| Tool                                          | Minimum version | Where to get it                                                   |
| --------------------------------------------- | --------------- | ----------------------------------------------------------------- |
| Visual Studio 2022                            | 17.6            | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| VS workload: **Desktop development with C++** | —               | VS Installer                                                      |
| cmake                                         | 3.24            | Included with the VS workload above                               |
| ninja                                         | any             | Included with the VS workload above                               |
| vcpkg                                         | any             | Included with VS 2022 17.6+ (see below)                           |
| git                                           | any             | [git-scm.com](https://git-scm.com/)                               |

### vcpkg

Visual Studio 2022 17.6 and later includes vcpkg. If you accepted the
recommended install options, it is already present. CMake will find it
automatically through the `VCPKG_INSTALLATION_ROOT` environment variable
that the VS Developer environment sets.

If you installed a standalone vcpkg separately, set `VCPKG_ROOT` to its
path and CMake will use that instead.

---

## Daily workflow in Visual Studio

### 1. Open the folder

Open Visual Studio 2022, then **File → Open → Folder…** and select the
project's root directory (the one containing `CMakeLists.txt`).

Do **not** open a `.sln` file from the `msvc-full-features/` directory —
that is the legacy build system. Both are separate; do not mix them.

### 2. Select a configuration

In the Standard toolbar, open the **Configuration** drop-down and choose:

| Configuration    | Use when                                      |
| ---------------- | --------------------------------------------- |
| `Debug`          | Debugging, all symbols, no optimisation       |
| `RelWithDebInfo` | Normal development — optimised but debuggable |
| `Release`        | Performance testing, distribution             |
| `Tests`          | Building and running the test suite           |
| `Tracy`          | Performance profiling with the Tracy profiler |

> **RelWithDebInfo** is the best default for day-to-day development.
> It is optimised (so the game runs at normal speed) but retains enough
> debug information for breakpoints and stack traces to work reliably.

The `Tests` configuration is a RelWithDebInfo build with the test suite
enabled. The other configurations have tests disabled to keep build times
shorter.

The `Tracy` configuration is a Release build with Tracy profiler
instrumentation compiled in. See [Tracy profiling](#tracy-profiling).

### 3. Build

**Build → Build All** (or `Ctrl+Shift+B`).

The first build downloads and compiles vcpkg dependencies, which takes a
while. Subsequent builds are incremental.

### 4. Run and debug

Set the startup item in the toolbar to the executable you want:

| Configuration                            | Startup item               |
| ---------------------------------------- | -------------------------- |
| Debug / RelWithDebInfo / Release / Tracy | **cataclysm-bn-tiles.exe** |
| Tests                                    | **cata_test-tiles.exe**    |

Then press **F5**.

The working directory is set to the project root via `launch.vs.json`,
so the game will find its data files without any additional setup.

---

## Customising your build

To override cmake variables for your local build, open `CMakeSettings.json`
and add entries to the `variables` array of the configuration you use.
The file is tracked by git, so for personal overrides either edit a local
branch or copy the configuration with a new name.

### Useful variables

| Variable      | Default                    | Effect                        |
| ------------- | -------------------------- | ----------------------------- |
| `TESTS`       | `OFF` (ON in Tests config) | Build the test suite          |
| `JSON_FORMAT` | `ON`                       | Build the JSON formatter tool |
| `LOCALIZE`    | `ON`                       | Build translation support     |
| `SOUND`       | `ON`                       | Build audio support           |

---

## Tracy profiling

[Tracy](https://github.com/wolfpld/tracy) is a real-time frame profiler.
Select the **Tracy** configuration in the VS toolbar and build normally.
Tracy uses `TRACY_ON_DEMAND` mode — the profiler connects and starts
recording only when the Tracy viewer attaches, so the game is usable
without the viewer running.

Tracy is also available through the terminal workflow using the
`windows-tiles-sounds-x64-msvc-tracy` cmake preset — see
[Terminal workflow](#terminal-workflow).

---

## Terminal workflow

`setup.ps1` validates prerequisites and configures the cmake preset used
for terminal builds. Run it once from a plain PowerShell window:

```powershell
.\setup.ps1
```

The script checks all prerequisites, downloads the gettext binaries needed
for building translations, and runs
`cmake --preset windows-tiles-sounds-x64-msvc`.

After that, the standard cmake commands work from a
**VS 2022 Developer Command Prompt** or **Developer PowerShell**:

```powershell
# Configure (one time, or after CMakeLists.txt changes)
cmake --preset windows-tiles-sounds-x64-msvc

# Build
cmake --build --preset windows-msvc-relwithdebinfo

# Run the game (from the project root directory)
.\out\build\windows-tiles-sounds-x64-msvc\src\RelWithDebInfo\cataclysm-bn-tiles.exe

# Run tests
.\out\build\windows-tiles-sounds-x64-msvc\tests\RelWithDebInfo\cata_test-tiles.exe

# Build translations only
cmake --build --preset windows-msvc-relwithdebinfo --target translations_compile

# Install (copies game + data to a self-contained directory)
cmake --install out\build\windows-tiles-sounds-x64-msvc --config RelWithDebInfo
```

> **Note:** `cmake --build` from a plain terminal (not a VS Developer
> terminal) relies on the baked VS environment in `CMakeUserPresets.json`.
> If that file is missing, run `setup.ps1` to regenerate it, or use a
> VS Developer Command Prompt instead.

---

## Troubleshooting

### CMake configure fails immediately

**Most common cause:** vcpkg is not found.

Check that `VCPKG_ROOT` is set (or that VS's bundled vcpkg is available).
Open a VS Developer Command Prompt and run:

```
echo %VCPKG_ROOT%
echo %VCPKG_INSTALLATION_ROOT%
```

At least one should point to a directory containing `vcpkg.exe`. If
neither is set, run `setup.ps1` — it locates the VS-bundled vcpkg
automatically.

### VS shows an `x64-Debug` configuration or the ncurses error appears

VS is not using `CMakeSettings.json`. Go to
**Tools → Options → CMake → General** and set the preset integration
to **"Use CMakeSettings.json (Legacy)"** or **"Never use CMake Presets"**.
Then do a full reset (see below).

### Configure succeeds but build fails with missing headers/libs

The VS environment (INCLUDE, LIB, PATH) may not have been captured
correctly. Try:

1. Delete `CMakeUserPresets.json` from the project root.
2. Delete the relevant `out\build\` subdirectory entirely.
3. Run `setup.ps1` again to regenerate both.

### Game launches but immediately crashes / can't find data

`launch.vs.json` at the project root sets the working directory to the
project root for F5 launches. If the file is missing or VS is not reading
it, the game may not find `./data/`.

If you are launching the `.exe` directly from File Explorer or a terminal,
make sure to run it from the project root directory:

```powershell
# Correct — run from the project root
.\out\build\windows-tiles-sounds-x64-msvc-relwithdebinfo\src\cataclysm-bn-tiles.exe

# Wrong — game can't find ./data/
cd out\build\windows-tiles-sounds-x64-msvc-relwithdebinfo\src
.\cataclysm-bn-tiles.exe
```

### "VsDevCmd.bat not found" error during configure

`VsDevCmd.bat` is inside your VS installation. If this error appears, VS
may be installed in a non-standard location. Set the `DevEnvDir`
environment variable to the path of your VS `Common7\IDE` directory before
running cmake:

```powershell
$env:DevEnvDir = "D:\VisualStudio\Common7\IDE\"
cmake --preset windows-tiles-sounds-x64-msvc
```

### Builds are very slow

ccache is detected and used automatically if it is installed and on
`PATH`. Install it from [ccache.dev](https://ccache.dev/) to dramatically
speed up incremental builds after `git clean` or branch switches.

### How to fully reset the build environment

If things are broken and you want a clean slate:

```powershell
# Delete VS's cached project state (stale configurations, IntelliSense DB)
Remove-Item -Recurse -Force .vs

# Delete all build output directories
Remove-Item -Recurse -Force out\build

# Delete the generated user presets (will be regenerated on next configure)
Remove-Item -Force CMakeUserPresets.json

# Re-run setup for terminal builds (VS will reconfigure itself on next open)
.\setup.ps1
```

### CMakeUserPresets.json shows "already exists" on reconfigure

This is intentional. The file is only generated on first configure to
avoid overwriting your customisations. If you want to reset it to the
default generated content, delete it and run `setup.ps1`.
