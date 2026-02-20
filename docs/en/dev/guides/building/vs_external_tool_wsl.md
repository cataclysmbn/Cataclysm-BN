# Visual Studio External Tool Automation (Windows + WSL)

This page documents the Visual Studio External Tool workflow used to run Cataclysm: BN CMake build
automation from a `cmd`/PowerShell interface.

## Overview

- The workflow is launched from Visual Studio's **External Tools** menu.
- The same flow drives Windows (MSVC) builds and WSL-based Linux builds.
- All selections are made through menus — no manual shell commands are needed.
- Full debugger support: Windows auto-attaches the VS debugger; Linux uses VS SSH attach.

## Add the external tool in Visual Studio

Open **Tools -> External Tools...** and add an entry. The **Arguments** field is the same
regardless of which interface mode you choose:

| Field             | Value                                |
|-------------------|--------------------------------------|
| Title             | BN Build                             |
| Command           | `cmd.exe`                            |
| Arguments         | `/c "$(SolutionDir)cmake-build.bat"` |
| Initial directory | `$(SolutionDir)`                     |

The **Use Output Window** checkbox controls which interface mode the tool uses:

| Use Output Window | Interface                                                                 |
|-------------------|---------------------------------------------------------------------------|
| **Unchecked**     | Opens a separate `cmd` window; numbered text menus appear inline          |
| **Checked**       | Runs inside the VS Output window; WinForms GUI list-pickers appear instead |

The GUI pickers (checked) keep everything inside VS but require clicking through floating windows.
The text menus (unchecked) open a separate console window where you type a number to select.
Both modes complete all the same steps — the choice is purely a preference.

![Visual Studio External Tools menu](https://github.com/user-attachments/assets/a7b5d4b8-2cd3-41be-98ae-e75997619a2c)

![External tool configuration example](https://github.com/user-attachments/assets/197c59df-ac2e-4e2a-99f4-8a5dab860367)

For a dedicated debug shortcut (e.g. "BN Debug"), add a second entry with the same settings but
append `-Action debug` to the Arguments field:

```
/c "$(SolutionDir)cmake-build.bat" -Platform win -Preset 1 -BuildType 2 -Action debug
```

## Interface modes

**Text menus** ("Use Output Window" unchecked): numbered prompts appear inline in the `cmd`
window. Enter the number for your selection.

![Prompt-driven cmd interface](https://github.com/user-attachments/assets/934ce9eb-37db-482d-b100-afd7fa215ed8)

**WinForms GUI pickers** ("Use Output Window" checked): floating list-box windows appear for each
selection. Click an item and press OK, or double-click to confirm.

> **Linux/WSL note:** GUI pickers apply only to the initial platform selection when building for
> Linux. After that, the script re-launches itself in an elevated administrator console (required
> for WSL operations), and all subsequent menus in that window use text prompts.

## Workflow

On each run the script prompts for:

1. **Platform** — Windows (MSVC) or Linux (WSL)
2. **Configure preset** — read from `CMakePresets.json` (and `CMakeUserPresets.json` if present)
3. **Build type** — Debug / RelWithDebInfo / Release *(Windows only; Linux type is set in the preset)*
4. **Target** — derived from the preset's `TILES`/`TESTS` cache variables, or enter a custom name
5. **Action** — Build, Run, Rebuild, Delete, or Debug

After a successful build the script offers to Run or Debug immediately without returning to the
main menu.

A "Repeat last" option is shown at the end of each session to re-run the same configuration
without re-answering every prompt.

![Build completion output](https://github.com/user-attachments/assets/b43f3130-77c0-4beb-91a0-3b98cb5915f8)

![Cataclysm: BN running after build](https://github.com/user-attachments/assets/3eaf7f95-5653-4c7d-acdd-7977c81ca0ee)

## Elevation

**Windows builds** run entirely at standard (unelevated) integrity. Do not run `cmake-build.bat`
or Visual Studio as administrator — this is required for the debugger auto-attach to work (see
below).

**Linux/WSL builds** require administrator privileges for WSL filesystem and network operations.
The script detects when it is not elevated and automatically spawns an elevated PowerShell window,
passing all selected options through so no re-selection is needed.

## Debugging

### Windows

Selecting the **Debug** action:

1. Launches the game executable at standard integrity with `Start-Process`.
2. Connects to the running Visual Studio instance via COM automation (the DTE object from the
   Running Object Table).
3. Calls `Debugger.Attach()` on the game process — VS attaches its native debugger automatically.

The script polls for up to 5 seconds for VS to register the process, then reports success or
prints fallback instructions.

**Requirements:**
- Visual Studio must be open and running at **standard** (non-elevated) integrity. If VS was
  launched as administrator the COM ROT is partitioned by integrity level and auto-attach will
  fail with a "no running VS instance found" message. Restart VS without elevation.
- `cmake-build.bat` must also run without elevation (the launcher does not elevate for Windows
  builds).

**If auto-attach fails**, the script prints the process PID. Attach manually via
**Debug → Attach to Process** (`Ctrl+Alt+P`) and find the process by name or PID.

### Linux (SSH attach)

Debugging a Linux WSL build uses Visual Studio's SSH remote-attach feature. The script
configures everything automatically on each debug run:

1. Installs `openssh-server` in WSL if not already present.
2. Enables `PasswordAuthentication yes` in `/etc/ssh/sshd_config`.
3. Runs `ssh-keygen -A` to generate any missing host keys.
4. Starts (or restarts) the SSH service.
5. Sets `/proc/sys/kernel/yama/ptrace_scope` to `0` — required so that GDB (used by VS) can
   attach to a process that is not a direct child of the debugger.
6. Resolves the current WSL IP address (WSL2 assigns a new IP on each start).
7. Removes any stale `netsh` port proxy and creates a fresh one:
   `Windows localhost:2222 → WSL <ip>:22`
8. Verifies that `localhost:2222` is reachable.
9. Writes a small launch script to `/tmp/` inside WSL and opens it in a **new WSL window** so
   the game has a real TTY and WSLg display environment (`DISPLAY`/`WAYLAND_DISPLAY`).
10. Prints step-by-step attach instructions.

**To attach in Visual Studio after the game window opens:**

1. Open **Debug → Attach to Process** (`Ctrl+Alt+P`).
2. Set **Connection type** to `SSH`.
3. Set **Connection target** to `localhost:2222`.
4. Press Enter or click the connect button; enter your WSL username and password when prompted.
5. Find the game process (e.g. `cataclysm-bn-tiles`) in the process list.
6. Click **Attach**.

Visual Studio saves the SSH connection after the first use. On subsequent debug runs you only need
to open "Attach to Process", select the saved `localhost:2222` connection, and attach.

> The port proxy uses `127.0.0.1` (loopback only), so no inbound Windows Firewall rules are
> required.

> `ptrace_scope=0` is set transiently (resets on WSL restart). It is not written to a persistent
> sysctl file, unlike the TSan `vm.mmap_rnd_bits` fix.

## Non-interactive shortcuts

All prompts can be bypassed by passing parameters directly to `cmake-build.bat`. This is useful
for additional External Tool entries that go straight to a specific action.

```bat
rem Windows MSVC build (preset 1, RelWithDebInfo)
cmake-build.bat -Platform win -Preset 1 -BuildType 2 -Action build

rem Windows debug (launch game and auto-attach VS debugger)
cmake-build.bat -Platform win -Preset 1 -BuildType 1 -Action debug

rem Linux WSL build (preset by name)
cmake-build.bat -Platform linux -Preset linux-slim -Target cataclysm-bn-tiles -Action build

rem Linux run with test filter
cmake-build.bat -Platform linux -Preset 2 -Action run -RunArgs "[map]"
```

**Parameter reference:**

| Parameter     | Values                                         | Notes                                      |
|---------------|------------------------------------------------|--------------------------------------------|
| `-Platform`   | `win` / `linux`                                |                                            |
| `-Preset`     | preset name or 1-based index                   | Indexes match the order in CMakePresets.json |
| `-BuildType`  | `1`=Debug `2`=RelWithDebInfo `3`=Release       | Windows only                               |
| `-Target`     | cmake target name                              | Derived from preset if omitted             |
| `-Action`     | `build` `run` `rebuild` `delete` `debug`       |                                            |
| `-RunArgs`    | string forwarded to the binary                 | e.g. `[map]` for test filtering            |
| `-ExtraFlags` | extra `cmake` configure flags                  | e.g. `-DFOO=ON`                            |
