<#
.SYNOPSIS
    One-time setup script for building Cataclysm: Bright Nights on Windows.

.DESCRIPTION
    Validates prerequisites (Visual Studio 2022, cmake, ninja, vcpkg, git) and runs
    the initial CMake configure step for terminal/CLI builds.

    Visual Studio IDE users do not need this script — VS configures itself
    automatically from CMakeSettings.json when you open the project folder.

    Run this from a plain PowerShell or VS Developer PowerShell prompt.
    Does not require administrator privileges.

.PARAMETER Preset
    The CMake configure preset to use. Defaults to "windows-tiles-sounds-x64-msvc".
    Use "windows-tiles-sounds-x64-msvc-tracy" to configure a Tracy-enabled build.

.EXAMPLE
    .\setup.ps1
    .\setup.ps1 -Preset windows-tiles-sounds-x64-msvc-tracy
#>

param(
    [string]$Preset = "windows-tiles-sounds-x64-msvc"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Write-Header([string]$Text) {
    Write-Host ""
    Write-Host "=== $Text ===" -ForegroundColor Cyan
}

function Write-Ok([string]$Text) {
    Write-Host "  [OK]   $Text" -ForegroundColor Green
}

function Write-Fail([string]$Text) {
    Write-Host "  [FAIL] $Text" -ForegroundColor Red
}

function Write-Info([string]$Text) {
    Write-Host "         $Text" -ForegroundColor Gray
}

$allOk = $true
$vsPath = $null

# ---------------------------------------------------------------------------
# Check: cmake
# ---------------------------------------------------------------------------
Write-Header "Checking prerequisites"

$cmakeVersion = $null
try {
    $cmakeRaw = cmake --version 2>&1 | Select-Object -First 1
    if ($cmakeRaw -match "cmake version (\d+\.\d+\.\d+)") {
        $cmakeVersion = [version]$Matches[1]
    }
} catch {}

if ($null -eq $cmakeVersion) {
    Write-Fail "cmake not found in PATH."
    Write-Info "Install cmake >= 3.24 from https://cmake.org/download/"
    Write-Info "or via the Visual Studio installer: modify your VS install and add"
    Write-Info "'C++ CMake tools for Windows' under Individual Components."
    $allOk = $false
} elseif ($cmakeVersion -lt [version]"3.24") {
    Write-Fail "cmake $cmakeVersion is too old (need >= 3.24)."
    Write-Info "Update cmake from https://cmake.org/download/"
    $allOk = $false
} else {
    Write-Ok "cmake $cmakeVersion"
}

# ---------------------------------------------------------------------------
# Check: Visual Studio 2022 with C++ workload
# ---------------------------------------------------------------------------
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vsWhere)) {
    Write-Fail "Visual Studio Installer not found."
    Write-Info "Install Visual Studio 2022 from https://visualstudio.microsoft.com/"
    Write-Info "Required workload: 'Desktop development with C++'"
    $allOk = $false
} else {
    $vsPath = & $vsWhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>&1
    if (-not $vsPath -or $vsPath -match "^Error") {
        Write-Fail "Visual Studio 2022 with C++ tools not found."
        Write-Info "In the Visual Studio Installer, ensure the"
        Write-Info "'Desktop development with C++' workload is installed."
        $allOk = $false
    } else {
        $vsVersion = & $vsWhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property catalog_productDisplayVersion 2>&1
        Write-Ok "Visual Studio $vsVersion at: $vsPath"
    }
}

# ---------------------------------------------------------------------------
# Check: vcpkg
# ---------------------------------------------------------------------------
$vcpkgExe = $null

if ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT "vcpkg.exe"))) {
    $vcpkgExe = Join-Path $env:VCPKG_ROOT "vcpkg.exe"
    Write-Ok "vcpkg (VCPKG_ROOT): $($env:VCPKG_ROOT)"
} elseif ($vsPath -and (Test-Path $vsWhere)) {
    # Try the vcpkg bundled with Visual Studio (VS 2022 17.6+).
    $vsBundledVcpkg = Join-Path $vsPath "VC\vcpkg"
    if (Test-Path (Join-Path $vsBundledVcpkg "vcpkg.exe")) {
        $vcpkgExe = Join-Path $vsBundledVcpkg "vcpkg.exe"
        $env:VCPKG_ROOT = $vsBundledVcpkg
        Write-Ok "vcpkg (VS bundled): $vsBundledVcpkg"
    }
}

if (-not $vcpkgExe) {
    Write-Fail "vcpkg not found."
    Write-Info "Option 1 (recommended): The VS installer includes vcpkg."
    Write-Info "           In the VS installer, add 'C++ package manager: vcpkg'."
    Write-Info "Option 2: Install vcpkg manually: https://vcpkg.io/en/getting-started.html"
    Write-Info "           Then set the VCPKG_ROOT environment variable to its path."
    $allOk = $false
}

# ---------------------------------------------------------------------------
# Check: git
# ---------------------------------------------------------------------------
try {
    $gitVersion = git --version 2>&1
    Write-Ok "git: $gitVersion"
} catch {
    Write-Fail "git not found in PATH."
    Write-Info "Install git from https://git-scm.com/ or via winget: winget install Git.Git"
    $allOk = $false
}

# ---------------------------------------------------------------------------
# Check: Ninja (required for Ninja Multi-Config generator)
# Ninja ships with VS 2022 via "C++ CMake tools for Windows".
# If it is not in PATH yet, find it from the VS install and add it so the
# cmake --preset invocation below can locate the generator.
# ---------------------------------------------------------------------------
$ninjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
if ($ninjaCmd) {
    $ninjaVersion = ninja --version 2>&1 | Select-Object -First 1
    Write-Ok "ninja $ninjaVersion"
} elseif ($vsPath) {
    $vsNinjaDir = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
    if (Test-Path (Join-Path $vsNinjaDir "ninja.exe")) {
        $env:PATH = "$vsNinjaDir;$env:PATH"
        $ninjaVersion = ninja --version 2>&1 | Select-Object -First 1
        Write-Ok "ninja $ninjaVersion (VS bundled)"
    } else {
        Write-Fail "ninja not found."
        Write-Info "Ninja ships with VS 2022 via the 'C++ CMake tools for Windows' component."
        Write-Info "In the VS Installer, ensure 'C++ CMake tools for Windows' is installed."
        $allOk = $false
    }
} else {
    Write-Fail "ninja not found."
    Write-Info "Ninja ships with VS 2022 via the 'C++ CMake tools for Windows' component."
    Write-Info "In the VS Installer, ensure 'C++ CMake tools for Windows' is installed."
    $allOk = $false
}

# ---------------------------------------------------------------------------
# Bail out if prerequisites are missing
# ---------------------------------------------------------------------------
if (-not $allOk) {
    Write-Host ""
    Write-Host "One or more prerequisites are missing. Fix the issues above and re-run setup.ps1." -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------------------
# Run cmake configure
# ---------------------------------------------------------------------------
Write-Header "Running CMake configure"
Write-Host "  Preset : $Preset"
Write-Host "  Output : out/build/$Preset"
Write-Host ""

cmake --preset $Preset

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Fail "CMake configure failed (exit code $LASTEXITCODE)."
    Write-Host ""
    Write-Host "Common fixes:" -ForegroundColor Yellow
    Write-Host "  - Delete out\build\$Preset and run setup.ps1 again."
    Write-Host "  - Run setup.ps1 from a VS 2022 Developer PowerShell prompt."
    Write-Host "  - Check that VCPKG_ROOT points to a valid vcpkg installation."
    Write-Host "  - Verify your internet connection (vcpkg downloads packages on first run)."
    exit 1
}

# ---------------------------------------------------------------------------
# Success
# ---------------------------------------------------------------------------
Write-Host ""
Write-Header "Setup complete"
Write-Ok "CMake configured successfully."
Write-Host ""
Write-Host "Terminal build commands:" -ForegroundColor Yellow
Write-Host "  cmake --build --preset windows-msvc-relwithdebinfo"
Write-Host "  cmake --build --preset windows-msvc-debug"
Write-Host "  cmake --build --preset windows-msvc-release"
Write-Host ""
Write-Host "Visual Studio IDE:" -ForegroundColor Yellow
Write-Host "  setup.ps1 is not required for the VS IDE workflow."
Write-Host "  Open this folder in VS 2022 (File > Open > Folder) and"
Write-Host "  select a configuration from the toolbar: Debug, RelWithDebInfo,"
Write-Host "  Release, Tests, or Tracy."
Write-Host ""
Write-Host "See docs/en/dev/guides/building/vs_cmake.md for full details."
Write-Host ""
