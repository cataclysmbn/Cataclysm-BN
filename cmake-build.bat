@echo off
rem cmake-build.bat — Launch cmake-build.ps1, auto-elevating to admin if needed.
rem
rem Usage:
rem   cmake-build.bat                                                     — interactive menus
rem   cmake-build.bat -Platform win -Preset 1 -BuildType 2 -Action build  — Windows MSVC build
rem   cmake-build.bat -Platform linux -Preset linux-slim -Action build     — Linux (WSL) build
rem   cmake-build.bat -Platform linux -Preset 2 -Action run -RunArgs "[map]"  — run with filter

net session >nul 2>&1
if %errorlevel% equ 0 goto :run

echo Set UAC = CreateObject("Shell.Application") > "%temp%\~elevate.vbs"
echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\~elevate.vbs"
cscript //nologo "%temp%\~elevate.vbs"
del "%temp%\~elevate.vbs"
exit /b

:run
powershell -ExecutionPolicy Bypass -File "%~dp0cmake-build.ps1" %*
pause
