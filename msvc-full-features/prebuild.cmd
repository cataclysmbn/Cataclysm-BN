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
if "%BUILD_TIMESTAMP%"=="" (
for /F "tokens=*" %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd-HHmm"') do set BUILD_TIMESTAMP=%%i
)
set NEED_REGEN=0
findstr /c:"#define VERSION \"%VERSION%\"" ..\src\version.h > NUL 2> NUL
if %ERRORLEVEL% NEQ 0 set NEED_REGEN=1
findstr /c:"#define BUILD_TIMESTAMP \"%BUILD_TIMESTAMP%\"" ..\src\version.h > NUL 2> NUL
if %ERRORLEVEL% NEQ 0 set NEED_REGEN=1
if %NEED_REGEN% NEQ 0 (
echo Generating "version.h"...
echo VERSION defined as "%VERSION%"
echo BUILD_TIMESTAMP defined as "%BUILD_TIMESTAMP%"
>..\src\version.h echo // NOLINT(cata-header-guard)
>>..\src\version.h echo #define VERSION "%VERSION%"
>>..\src\version.h echo #define BUILD_TIMESTAMP "%BUILD_TIMESTAMP%"
)
