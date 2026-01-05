@echo off
SETLOCAL

cd ..\msvc-full-features
set PATH=%PATH%;%VSAPPIDDIR%\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd
if "%VERSION%"=="" (
for /F "tokens=*" %%i in ('git describe --tags --always --dirty --match "[0-9]*.*"') do set VERSION=%%i
)
if "%VERSION%"=="" (
set VERSION=Please install `git` to generate VERSION, or set the variable manually
)
findstr /c:%VERSION% ..\src\version.h > NUL 2> NUL
if %ERRORLEVEL% NEQ 0 (
echo Generating "version.h"...
echo VERSION defined as "%VERSION%"
>..\src\version.h echo #define VERSION "%VERSION%"
)
