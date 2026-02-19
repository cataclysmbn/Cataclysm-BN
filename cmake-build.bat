@echo off
rem cmake-build.bat — Launch cmake-build.ps1.
rem
rem Usage:
rem   cmake-build.bat                                                     — interactive menus
rem   cmake-build.bat -Platform win -Preset 1 -BuildType 2 -Action build  — Windows MSVC build
rem   cmake-build.bat -Platform linux -Preset linux-slim -Action build     — Linux (WSL) build
rem   cmake-build.bat -Platform linux -Preset 2 -Action run -RunArgs "[map]"  — run with filter
rem   cmake-build.bat -Platform win -Preset 1 -BuildType 2 -Action debug  — run under VS debugger
rem
rem No elevation is required. WSL operations use "wsl -u root" (Linux-side root, not Windows admin).
rem cmake-build and Visual Studio both run at standard integrity level, which is required for the
rem VS debugger auto-attach (COM ROT is partitioned by integrity level).

powershell -Sta -ExecutionPolicy Bypass -File "%~dp0cmake-build.ps1" %*
pause
