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
rem Windows builds run at standard integrity level (required for VS debugger auto-attach via COM ROT).
rem Linux/WSL builds auto-elevate: the script spawns an elevated PowerShell window for WSL operations.

powershell -Sta -ExecutionPolicy Bypass -File "%~dp0cmake-build.ps1" %*
