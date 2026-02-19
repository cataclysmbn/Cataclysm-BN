# cmake-build.ps1 -- Build Cataclysm-BN using cmake presets
#
# REQUIREMENTS:
#   Windows builds : cmake in PATH (included with Visual Studio 2022)
#   Linux builds   : WSL2 with Ubuntu (build dependencies installed automatically)
#
# Configure presets are read from CMakePresets.json (and CMakeUserPresets.json
# if it exists). No build settings are hardcoded in this script.
#
# VISUAL STUDIO EXTERNAL TOOL SETUP (manual, one-time)
# Tools -> External Tools -> Add:
#   Title:             BN Build
#   Command:           cmd.exe
#   Arguments:         /c "$(SolutionDir)cmake-build.bat"
#   Initial directory: $(SolutionDir)
#   Use Output Window: unchecked  (opens a separate window; required for menus)
#
# Non-interactive shortcut examples:
#   /c "$(SolutionDir)cmake-build.bat -Platform win -Preset 1 -BuildType 2 -Action build"
#   /c "$(SolutionDir)cmake-build.bat -Platform linux -Preset linux-slim -Target cataclysm-bn-tiles -Action build"

param(
    [string]$Platform   = "",   # win | linux | mac
    [string]$Preset     = "",   # configure preset name or selection number
    [string]$BuildType  = "",   # Windows only: 1=Debug 2=RelWithDebInfo 3=Release
    [string]$Target     = "",   # cmake build target (derived from preset if blank)
    [string]$Action     = "",   # build | run | rebuild | delete
    [string]$RunArgs    = "",   # forwarded verbatim to the binary when running
    [string]$ExtraFlags = ""    # extra cmake configure flags, e.g. -DFOO=ON
)

# ── User configuration ────────────────────────────────────────────────────────
$WslSrcDir = "~/cbn"        # WSL path for synced source
$WslBldDir = "~/cbn-build"  # WSL path for build dirs (overrides preset binaryDir)
$VcpkgRoot = ""             # Windows only: path to vcpkg root (auto-detected if blank)

# ── Path resolution ───────────────────────────────────────────────────────────
$WinSrcPath = $PSScriptRoot
$DriveLetter = $WinSrcPath.Substring(0, 1).ToLower()
$WslSrcPath  = "/mnt/$DriveLetter$($WinSrcPath.Substring(2).Replace('\', '/'))"

# ── Load cmake presets ────────────────────────────────────────────────────────
$presetsFile = "$WinSrcPath\CMakePresets.json"
if (-not (Test-Path $presetsFile)) {
    Write-Error "CMakePresets.json not found at $presetsFile"
    exit 1
}
$presetsData = Get-Content $presetsFile -Raw | ConvertFrom-Json
$allConfigPresets = @($presetsData.configurePresets)
$allBuildPresets  = @($presetsData.buildPresets)

# Merge CMakeUserPresets.json if present (VS manages this file; don't edit it manually)
$userPresetsFile = "$WinSrcPath\CMakeUserPresets.json"
if (Test-Path $userPresetsFile) {
    $userData = Get-Content $userPresetsFile -Raw | ConvertFrom-Json
    if ($userData.configurePresets) { $allConfigPresets += @($userData.configurePresets) }
    if ($userData.buildPresets)     { $allBuildPresets  += @($userData.buildPresets) }
}

# Walk the inheritance chain to find the first non-null value of a named field.
function Get-PresetField($PresetName, $Field) {
    $p = $allConfigPresets | Where-Object { $_.name -eq $PresetName } | Select-Object -First 1
    if (-not $p) { return $null }
    # Use -in on the Names collection rather than .Item() — more reliable in PS5.1
    # against PSCustomObject instances created by ConvertFrom-Json.
    if ($Field -in $p.PSObject.Properties.Name) {
        $val = $p.$Field
        if ($null -ne $val) { return $val }
    }
    if ($p.inherits) {
        $parents = if ($p.inherits -is [array]) { $p.inherits } else { @($p.inherits) }
        foreach ($parent in $parents) {
            $v = Get-PresetField $parent $Field
            if ($null -ne $v) { return $v }
        }
    }
    return $null
}

# Merge cacheVariables along the full inheritance chain (child wins over parent).
function Get-CacheVars($PresetName) {
    $p = $allConfigPresets | Where-Object { $_.name -eq $PresetName } | Select-Object -First 1
    $result = @{}
    if (-not $p) { return $result }
    if ($p.inherits) {
        $parents = if ($p.inherits -is [array]) { $p.inherits } else { @($p.inherits) }
        foreach ($parent in $parents) {
            foreach ($kv in (Get-CacheVars $parent).GetEnumerator()) { $result[$kv.Key] = $kv.Value }
        }
    }
    if ($p.cacheVariables) {
        foreach ($prop in $p.cacheVariables.PSObject.Properties) { $result[$prop.Name] = $prop.Value }
    }
    return $result
}

# Resolve the preset's binaryDir, substituting ${sourceDir} and ${presetName}.
function Resolve-BinaryDir($PresetName, $SrcDir) {
    $template = Get-PresetField $PresetName "binaryDir"
    if (-not $template) { return "$SrcDir/out/build/$PresetName" }
    return $template -replace '\$\{sourceDir\}', $SrcDir `
                     -replace '\$\{presetName\}', $PresetName
}

# Derive the WSL packages required by a preset's resolved cacheVariables.
# This is best-effort: complex presets (e.g. with Tracy) may need extras.
function Get-RequiredPackages($CacheVars) {
    $pkgs = @("cmake", "rsync", "zlib1g-dev", "libsqlite3-dev", "ninja-build")

    # Compiler
    $cc  = "$($CacheVars['CMAKE_C_COMPILER'])"
    $cxx = "$($CacheVars['CMAKE_CXX_COMPILER'])"
    if ($cxx -match "g\+\+-(\d+)") {
        $pkgs += @("gcc-$($Matches[1])", "g++-$($Matches[1])")
    } elseif ($cc -match "gcc-(\d+)") {
        $pkgs += @("gcc-$($Matches[1])", "g++-$($Matches[1])")
    } elseif ($cc -match "clang" -or $cxx -match "clang") {
        $pkgs += "clang"
    } else {
        $pkgs += @("gcc", "g++")   # fallback
    }

    # llvm tools (llvm-ar, llvm-ranlib)
    if ("$($CacheVars['CMAKE_AR'])" -match "llvm") { $pkgs += "llvm" }

    # ccache
    if ("$($CacheVars['CMAKE_C_COMPILER_LAUNCHER'])" -match "ccache") { $pkgs += "ccache" }

    # Linker
    if ("$($CacheVars['LINKER'])" -match "mold") { $pkgs += "mold" }

    # SDL2 or ncurses
    $hasTiles = "$($CacheVars['TILES'])" -match "^(ON|True|1)$"
    if ($hasTiles) {
        $pkgs += @("libsdl2-dev", "libsdl2-image-dev", "libsdl2-mixer-dev",
                   "libsdl2-ttf-dev", "libfreetype-dev")
    } else {
        $pkgs += "libncurses-dev"
    }

    return $pkgs
}

# Derive sensible build targets from a preset's resolved cacheVariables.
function Get-PresetTargets($CacheVars) {
    $tiles = "$($CacheVars['TILES'])" -match "^(ON|True|1)$"
    $tests = "$($CacheVars['TESTS'])" -match "^(ON|True|1)$"
    $targets = @()
    if ($tests) { $targets += if ($tiles) { "cata_test-tiles" } else { "cata_test" } }
    $targets += if ($tiles) { "cataclysm-bn-tiles" } else { "cataclysm-bn" }
    return $targets
}

# Locate a vcpkg installation on Windows. Returns the root path or $null.
# Detection order:
#   1. $VcpkgRoot user config (top of this script)
#   2. VCPKG_ROOT environment variable (already set by user or CI)
#   3. VCPKG_INSTALLATION_ROOT environment variable (GitHub Actions)
#   4. %LOCALAPPDATA%\vcpkg\vcpkg.path.txt — written by "vcpkg integrate install";
#      this is how Visual Studio finds vcpkg when you run it through the IDE
#   5. Common manual install paths
function Find-Vcpkg {
    $candidates = @()
    if ($VcpkgRoot)                        { $candidates += $VcpkgRoot }
    if ($env:VCPKG_ROOT)                   { $candidates += $env:VCPKG_ROOT }
    if ($env:VCPKG_INSTALLATION_ROOT)      { $candidates += $env:VCPKG_INSTALLATION_ROOT }

    # vcpkg integrate install writes the path here; VS reads it the same way
    $pathFile = "$env:LOCALAPPDATA\vcpkg\vcpkg.path.txt"
    if (Test-Path $pathFile) {
        $fromFile = (Get-Content $pathFile -Raw).Trim()
        if ($fromFile) { $candidates += $fromFile }
    }

    # VS 2022 17.6+ ships a bundled vcpkg under the VS install tree
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath 2>$null
        if ($vsPath) { $candidates += "$vsPath\VC\vcpkg" }
    }

    $candidates += @("C:\vcpkg", "C:\src\vcpkg", "C:\dev\vcpkg",
                     "$env:USERPROFILE\vcpkg", "$env:USERPROFILE\source\vcpkg")

    foreach ($c in $candidates) {
        if ($c -and (Test-Path "$c\vcpkg.exe")) { return $c }
    }
    return $null
}

# Ensure gettext (msgfmt.exe) is available on Windows.
# Required when the preset has LOCALIZE=True (lang/CMakeLists.txt checks for it).
# If not found in PATH or a previous local install, downloads and silently
# installs the static-64 release from https://github.com/mlocati/gettext-iconv-windows.
function Ensure-Gettext {
    param([string]$InstallDir = "$env:LOCALAPPDATA\CataclysmBN\gettext")

    # Already in PATH?
    $existing = Get-Command msgfmt.exe -ErrorAction SilentlyContinue
    if ($existing) {
        Write-Host "--- gettext : $($existing.Source)"
        return
    }

    # Previously installed by this script into the local directory?
    if (Test-Path "$InstallDir\bin\msgfmt.exe") {
        $env:PATH = "$InstallDir\bin;$env:PATH"
        Write-Host "--- gettext : $InstallDir (cached)"
        return
    }

    Write-Host "--- gettext not found; downloading from mlocati/gettext-iconv-windows..."
    try {
        $release = Invoke-RestMethod "https://api.github.com/repos/mlocati/gettext-iconv-windows/releases/latest" `
                       -Headers @{ "User-Agent" = "build-wsl.ps1" }
        $asset   = @($release.assets | Where-Object { $_.name -match "gettext.*-static-64\.exe" })[0]
        if (-not $asset) { throw "No static-64 asset in latest release." }
    } catch {
        Write-Warning "Could not fetch gettext release info: $_"
        Write-Warning "Install gettext manually and add its bin\ folder to PATH."
        Write-Warning "Download: https://github.com/mlocati/gettext-iconv-windows/releases"
        return   # non-fatal; cmake will surface its own error if truly needed
    }

    $installer = "$env:TEMP\$($asset.name)"
    $sizeMB    = [math]::Round($asset.size / 1MB, 1)
    Write-Host "    Downloading $($asset.name) ($sizeMB MB)..."
    try {
        Invoke-WebRequest $asset.browser_download_url -OutFile $installer -UseBasicParsing
    } catch {
        Write-Warning "Download failed: $_"
        return
    }

    Write-Host "    Installing to $InstallDir (no admin required)..."
    # NSIS silent install flags: no UI, no reboot prompt, custom destination
    Start-Process $installer `
        -ArgumentList "/VERYSILENT /NORESTART /SUPPRESSMSGBOXES /DIR=`"$InstallDir`"" `
        -Wait
    Remove-Item $installer -ErrorAction SilentlyContinue

    if (Test-Path "$InstallDir\bin\msgfmt.exe") {
        $env:PATH = "$InstallDir\bin;$env:PATH"
        Write-Host "--- gettext installed: $InstallDir"
    } else {
        Write-Warning "gettext installation may have failed. If cmake reports 'Gettext not found', install manually."
    }
}

# ── Windows build types (MSVC multi-config: type selected at build time) ──────
$WinBuildTypes = @("Debug", "RelWithDebInfo", "Release")

# ── Classify presets by platform ──────────────────────────────────────────────
# Resolve generators up-front with a foreach; avoids calling Get-PresetField
# (which itself runs a Where-Object on $allConfigPresets) from within another
# Where-Object on the same collection — a nested-pipeline pattern that can
# silently misbehave in PS5.1.
$presetGeneratorMap = @{}
foreach ($p in $allConfigPresets) {
    $presetGeneratorMap[$p.name] = "$( Get-PresetField $p.name 'generator' )"
}

# Return "win" | "linux" | "mac" | "android" for a configure preset.
# Priority: generator field → CMAKE_SYSTEM_NAME cacheVar → name keywords → default linux.
function Get-PresetPlatform($PresetName) {
    if ($presetGeneratorMap[$PresetName] -match "Visual Studio") { return "win" }

    $cv = Get-CacheVars $PresetName
    $sysName = "$($cv['CMAKE_SYSTEM_NAME'])"
    if ($sysName -match "Darwin")  { return "mac"     }
    if ($sysName -match "Android") { return "android" }

    $lower = $PresetName.ToLower()
    if ($lower -match "mac|macos|darwin|osx") { return "mac"     }
    if ($lower -match "android")               { return "android" }

    return "linux"
}

$winPresets   = @($allConfigPresets | Where-Object { (Get-PresetPlatform $_.name) -eq "win"   })
$linuxPresets = @($allConfigPresets | Where-Object { (Get-PresetPlatform $_.name) -eq "linux" })
$macPresets   = @($allConfigPresets | Where-Object { (Get-PresetPlatform $_.name) -eq "mac"   })
# Android presets exist in some forks but cannot be built via this script.

# ── Loop setup ────────────────────────────────────────────────────────────────
# Save original params so each "Start fresh" iteration resets to them.
$ParamPlatform   = $Platform
$ParamPreset     = $Preset
$ParamBuildType  = $BuildType
$ParamTarget     = $Target
$ParamAction     = $Action
$ParamRunArgs    = $RunArgs
$ParamExtraFlags = $ExtraFlags

$lastConfig = $null

while ($true) {

# ── Platform selection ────────────────────────────────────────────────────────
# Build the menu dynamically; macOS only appears when mac presets are present.
$platformList = [ordered]@{}
$platformList["win"]   = "Windows  (MSVC - native .exe)"
$platformList["linux"] = "Linux    (WSL - for TSan and Linux builds)"
if ($macPresets.Count -gt 0) { $platformList["mac"] = "macOS    (native cmake)" }

if ($Platform -ne "") {
    if (-not $platformList.Contains($Platform)) {
        Write-Error "Invalid -Platform '$Platform'. Valid: $($platformList.Keys -join ', ')."
        exit 1
    }
} else {
    Write-Host ""
    Write-Host "Select platform:"
    $pKeys = @($platformList.Keys)
    for ($i = 0; $i -lt $pKeys.Count; $i++) {
        Write-Host "  $($i + 1)  $($platformList[$pKeys[$i]])"
    }
    Write-Host ""
    $pidx = [int](Read-Host "Enter number") - 1
    if ($pidx -ge 0 -and $pidx -lt $pKeys.Count) { $Platform = $pKeys[$pidx] }
    else { Write-Error "Invalid selection."; exit 1 }
}
$IsWin   = ($Platform -eq "win")
$IsMac   = ($Platform -eq "mac")
$IsLinux = ($Platform -eq "linux")

# ── Platform availability check ───────────────────────────────────────────────
$cmakeExe = $null   # set below for win/mac; unused for linux (wsl cmake)

if ($IsWin) {
    # Prefer cmake from PATH; fall back to the copy bundled with Visual Studio.
    $cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCmd) {
        $cmakeExe = $cmakeCmd.Source
    } else {
        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vsWhere) {
            $vsPath  = & $vsWhere -latest -property installationPath 2>$null
            $vsCmake = "$vsPath\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $vsCmake) { $cmakeExe = $vsCmake }
        }
    }
    if (-not $cmakeExe) {
        Write-Error "cmake not found. Ensure Visual Studio 2022 is installed, or add cmake to PATH."
        exit 1
    }

    # vcpkg provides SDL2 and all other Windows dependencies.
    # MSVC.cmake only activates vcpkg integration when VCPKG_ROOT is set.
    $detectedVcpkg = Find-Vcpkg
    if ($detectedVcpkg) {
        $env:VCPKG_ROOT = $detectedVcpkg
        Write-Host "--- vcpkg : $detectedVcpkg"
    } else {
        Write-Host ""
        Write-Error @"
vcpkg not found. Windows dependencies (SDL2, etc.) are provided by vcpkg.
To fix, do ONE of the following:
  1. Set the VCPKG_ROOT environment variable to your vcpkg installation path.
  2. Run 'vcpkg integrate install' so Visual Studio and this script can find it.
  3. Set `$VcpkgRoot at the top of build-wsl.ps1 to the vcpkg path.
  4. Pass the path as an extra flag: -ExtraFlags "-DVCPKG_ROOT=C:\path\to\vcpkg"
"@
        exit 1
    }

    if ($winPresets.Count -eq 0) {
        Write-Error "No Windows configure presets found in CMakePresets.json."
        exit 1
    }
} elseif ($IsMac) {
    # macOS: must be running on macOS (PS7+ sets $IsMacOS); cmake must be in PATH.
    $runningOnMac = ($null -ne (Get-Variable IsMacOS -ErrorAction SilentlyContinue)) -and $IsMacOS
    if (-not $runningOnMac) {
        Write-Error "macOS builds can only be run on macOS. This script is currently running on Windows."
        exit 1
    }
    $cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCmd) { $cmakeExe = $cmakeCmd.Source }
    if (-not $cmakeExe) {
        Write-Error "cmake not found. Install via Homebrew: brew install cmake"
        exit 1
    }
    if ($macPresets.Count -eq 0) {
        Write-Error "No macOS configure presets found."
        exit 1
    }
} else {
    # Linux (WSL)
    wsl true 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Error "WSL not available. Install WSL2 with Ubuntu from the Microsoft Store."
        exit 1
    }
    if ($linuxPresets.Count -eq 0) {
        Write-Error "No Linux configure presets found in CMakePresets.json."
        exit 1
    }
}

# ── Preset selection ──────────────────────────────────────────────────────────
$availPresets   = @(if ($IsWin) { $winPresets } elseif ($IsMac) { $macPresets } else { $linuxPresets })
$selectedPreset = $null

if ($Preset -ne "") {
    $asInt = 0
    if ([int]::TryParse($Preset, [ref]$asInt)) {
        $idx = $asInt - 1
        if ($idx -ge 0 -and $idx -lt $availPresets.Count) { $selectedPreset = $availPresets[$idx] }
        else { Write-Error "Invalid -Preset '$Preset'. Range: 1-$($availPresets.Count)."; exit 1 }
    } else {
        $selectedPreset = $availPresets | Where-Object { $_.name -eq $Preset } | Select-Object -First 1
        if (-not $selectedPreset) { Write-Error "Preset '$Preset' not found."; exit 1 }
    }
} else {
    Write-Host ""
    Write-Host "Select configure preset:"
    for ($i = 0; $i -lt $availPresets.Count; $i++) {
        $label = if ($availPresets[$i].displayName) { $availPresets[$i].displayName } else { $availPresets[$i].name }
        Write-Host "  $($i + 1)  $label"
    }
    Write-Host ""
    $idx = [int](Read-Host "Enter number") - 1
    if ($idx -ge 0 -and $idx -lt $availPresets.Count) { $selectedPreset = $availPresets[$idx] }
    else { Write-Error "Invalid selection."; exit 1 }
}

$presetName = $selectedPreset.name
$cacheVars  = Get-CacheVars $presetName

# ── Windows: build type selection ────────────────────────────────────────────
$selectedBuildType = $null
if ($IsWin) {
    if ($BuildType -ne "") {
        $idx = [int]$BuildType - 1
        if ($idx -ge 0 -and $idx -lt $WinBuildTypes.Count) { $selectedBuildType = $WinBuildTypes[$idx] }
        else { Write-Error "Invalid -BuildType '$BuildType'. Range: 1-$($WinBuildTypes.Count)."; exit 1 }
    } else {
        Write-Host ""
        Write-Host "Select build type:"
        for ($i = 0; $i -lt $WinBuildTypes.Count; $i++) { Write-Host "  $($i + 1)  $($WinBuildTypes[$i])" }
        Write-Host ""
        $idx = [int](Read-Host "Enter number") - 1
        if ($idx -ge 0 -and $idx -lt $WinBuildTypes.Count) { $selectedBuildType = $WinBuildTypes[$idx] }
        else { Write-Error "Invalid selection."; exit 1 }
    }
}

# ── Target selection ──────────────────────────────────────────────────────────
$inferredTargets = Get-PresetTargets $cacheVars
$selectedTarget  = $null

if ($Target -ne "") {
    $selectedTarget = $Target
} elseif ($inferredTargets.Count -eq 1) {
    $selectedTarget = $inferredTargets[0]
} else {
    Write-Host ""
    Write-Host "Select target:"
    for ($i = 0; $i -lt $inferredTargets.Count; $i++) { Write-Host "  $($i + 1)  $($inferredTargets[$i])" }
    Write-Host "  $($inferredTargets.Count + 1)  Enter a custom target name"
    Write-Host ""
    $choice = Read-Host "Enter number"
    $idx = [int]$choice - 1
    if ($idx -ge 0 -and $idx -lt $inferredTargets.Count) {
        $selectedTarget = $inferredTargets[$idx]
    } elseif ($idx -eq $inferredTargets.Count) {
        $selectedTarget = Read-Host "Target name"
    } else {
        Write-Error "Invalid selection."
        exit 1
    }
}

$isTestTarget = $selectedTarget -match "^cata_test"

# ── Derived values ────────────────────────────────────────────────────────────
if ($IsWin) {
    # MSVC multi-config: one build dir, config selected at build time.
    # Binary lands in <builddir>\<MSVCConfig>\<name>.exe
    $winBuildDir = Resolve-BinaryDir $presetName $WinSrcPath
    $winBuildDir = $winBuildDir -replace '/', '\'
    $buildDir    = $winBuildDir
    $winSubdir   = if ($isTestTarget) { "tests" } else { "src" }
    $binaryPath  = "$winBuildDir\$winSubdir\$selectedBuildType\$selectedTarget.exe"
    $buildTypeLabel = $selectedBuildType
} elseif ($IsMac) {
    # macOS single-config: use preset's binaryDir (resolved against source path).
    # Binary location follows same CMakeLists.txt rules as Linux:
    #   Debug/TSan -> source dir; others -> <builddir>/<subdir>/
    $macBuildPath = Resolve-BinaryDir $presetName ($WinSrcPath.Replace('\', '/'))
    $buildDir     = $macBuildPath
    $buildType    = "$($cacheVars['CMAKE_BUILD_TYPE'])"
    $isTSan       = "$($cacheVars['CMAKE_CXX_FLAGS'])" -match "fsanitize=thread"
    $binaryInSrcDir = ($buildType -eq "Debug") -or $isTSan

    if ($binaryInSrcDir) {
        $binaryPath = "$WinSrcPath/$selectedTarget"
    } else {
        $subdir = if ($isTestTarget) { "tests" } else { "src" }
        $binaryPath = "$macBuildPath/$subdir/$selectedTarget"
    }
    $buildTypeLabel = if ($buildType) { $buildType } else { "(from preset)" }
} else {
    # Linux single-config: store builds in $WslBldDir/$presetName (overrides preset binaryDir).
    # Binary location follows CMakeLists.txt rules:
    #   Debug/TSan -> CMAKE_RUNTIME_OUTPUT_DIRECTORY = CMAKE_SOURCE_DIR (source tree)
    #   Others     -> <builddir>/<subdir>/ where <subdir> is where the target is defined
    $wslBuildPath = "$WslBldDir/$presetName"
    $buildDir     = $wslBuildPath
    $buildType    = "$($cacheVars['CMAKE_BUILD_TYPE'])"
    $isTSan       = "$($cacheVars['CMAKE_CXX_FLAGS'])" -match "fsanitize=thread"
    $binaryInSrcDir = ($buildType -eq "Debug") -or $isTSan

    if ($binaryInSrcDir) {
        $binaryPath = "$WslSrcDir/$selectedTarget"
    } else {
        $subdir = if ($isTestTarget) { "tests" } else { "src" }
        $binaryPath = "$wslBuildPath/$subdir/$selectedTarget"
    }
    $buildTypeLabel = if ($buildType) { $buildType } else { "(from preset)" }
}

# ── Action selection ──────────────────────────────────────────────────────────
$validActions = @("build", "run", "rebuild", "delete")
if ($Action -ne "" -and $validActions -notcontains $Action) {
    Write-Error "Invalid -Action '$Action'. Valid: $($validActions -join ', ')."
    exit 1
}
if ($Action -eq "") {
    Write-Host ""
    Write-Host "Select action:"
    Write-Host "  1  Build"
    Write-Host "  2  Run"
    Write-Host "  3  Rebuild  (wipe build dir, reconfigure, build)"
    Write-Host "  4  Delete build"
    Write-Host "  x  Exit"
    Write-Host ""
    switch (Read-Host "Enter number") {
        "1" { $Action = "build"   }
        "2" { $Action = "run"     }
        "3" { $Action = "rebuild" }
        "4" { $Action = "delete"  }
        "x" { exit 0             }
        default { Write-Error "Invalid selection."; exit 1 }
    }
}

if ($isTestTarget -and $Action -eq "run" -and $RunArgs -eq "") {
    Write-Host ""
    $RunArgs = Read-Host "Test args (blank = run all, e.g. [map])"
}

if (($Action -eq "build" -or $Action -eq "rebuild") -and $ExtraFlags -eq "") {
    Write-Host ""
    $ExtraFlags = Read-Host "Extra cmake flags (blank = none, e.g. -DFOO=ON)"
}

# ── Summary ───────────────────────────────────────────────────────────────────
$presetLabel = if ($selectedPreset.displayName) { $selectedPreset.displayName } else { $presetName }
Write-Host ""
Write-Host "==> Platform  : $(if ($IsWin) { 'Windows (MSVC)' } elseif ($IsMac) { 'macOS' } else { 'Linux (WSL)' })"
Write-Host "==> Preset    : $presetLabel  [$presetName]"
Write-Host "==> Build type: $buildTypeLabel"
Write-Host "==> Target    : $selectedTarget"
Write-Host "==> Action    : $Action"
Write-Host "==> Build dir : $buildDir"
Write-Host "==> Binary    : $binaryPath"
if ($ExtraFlags -ne "") { Write-Host "==> Extra flags: $ExtraFlags" }
if ($RunArgs    -ne "") { Write-Host "==> Run args  : $RunArgs" }
Write-Host ""

# ── Delete ────────────────────────────────────────────────────────────────────
if ($Action -eq "delete") {
    $confirm = Read-Host "Delete $buildDir? (y/N)"
    if ($confirm -ne "y" -and $confirm -ne "Y") { Write-Host "Cancelled."; continue }
    Write-Host "--- Deleting $buildDir ..."
    if ($IsLinux) {
        wsl bash -c "rm -rf $wslBuildPath"
        if ($LASTEXITCODE -ne 0) { Write-Error "Delete failed."; exit 1 }
    } else {
        Remove-Item -Recurse -Force $buildDir 2>$null
        if (Test-Path $buildDir) { Write-Error "Delete failed."; exit 1 }
    }
    Write-Host "==> Deleted."
}

# ── Build ─────────────────────────────────────────────────────────────────────
$savedAction = $Action   # capture before post-build prompt may mutate $Action to "run"
if ($Action -eq "build" -or $Action -eq "rebuild") {

    if ($IsWin) {
        # ── Windows (MSVC) build ───────────────────────────────────────────────

        if ($Action -eq "rebuild") {
            Write-Host "--- Wiping build dir..."
            Remove-Item -Recurse -Force $winBuildDir 2>$null
            Write-Host ""
        }

        $cacheExists = Test-Path "$winBuildDir\CMakeCache.txt"

        if ($cacheExists -and $ExtraFlags -ne "") {
            Write-Host "--- Extra flags only apply during configure, but a cached build exists."
            $r = Read-Host "Rebuild from scratch to apply them? [Y/n]"
            if ($r -eq "" -or $r -eq "y" -or $r -eq "Y") {
                Write-Host "--- Wiping build dir..."
                Remove-Item -Recurse -Force $winBuildDir 2>$null
                $cacheExists = $false
            }
            Write-Host ""
        }

        if (-not $cacheExists) {
            # gettext (msgfmt) is required when LOCALIZE is enabled.
            if ("$($cacheVars['LOCALIZE'])" -match "^(True|ON|1)$") {
                Ensure-Gettext
            }

            Write-Host "--- Configuring ($presetName)..."
            Push-Location $WinSrcPath
            & $cmakeExe --preset $presetName
            $cfgResult = $LASTEXITCODE
            # Apply extra flags as a cache update pass after the initial configure
            if ($cfgResult -eq 0 -and $ExtraFlags -ne "") {
                & $cmakeExe $winBuildDir $ExtraFlags
                $cfgResult = $LASTEXITCODE
            }
            Pop-Location
            if ($cfgResult -ne 0) { Write-Error "cmake configure failed."; exit 1 }
            Write-Host ""
        } else {
            Write-Host "--- Skipping configure (cache exists). Choose Rebuild to reconfigure."
            Write-Host ""
        }

        # Suppress the post-build deno docs:gen step — it rewrites cli_options.md and
        # other tracked files, creating unwanted source-control noise for local builds.
        # (cmake cache update; no-op if deno is not installed)
        & $cmakeExe $winBuildDir -DLUA_DOCS_ON_BUILD:BOOL=OFF 2>$null | Out-Null

        Write-Host "--- Building $selectedTarget ($selectedBuildType)..."
        & $cmakeExe --build $winBuildDir --config $selectedBuildType --target $selectedTarget
        if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit 1 }
        Write-Host ""
        Write-Host "==> Build complete."
        Write-Host "==> Binary: $binaryPath"
        Write-Host ""
        $r = Read-Host "Run $selectedTarget now? [Y/n]"
        if ($r -eq "" -or $r -eq "y" -or $r -eq "Y") {
            if ($isTestTarget -and $RunArgs -eq "") {
                Write-Host ""
                $RunArgs = Read-Host "Test args (blank = run all, e.g. [map])"
            }
            $Action = "run"
        }
        Write-Host ""

    } elseif ($IsMac) {
        # ── macOS (native cmake) build ────────────────────────────────────────

        if ($Action -eq "rebuild") {
            Write-Host "--- Wiping build dir..."
            Remove-Item -Recurse -Force $macBuildPath -ErrorAction SilentlyContinue
            Write-Host ""
        }

        $cacheExists = Test-Path "$macBuildPath/CMakeCache.txt"

        if ($cacheExists -and $ExtraFlags -ne "") {
            Write-Host "--- Extra flags only apply during configure, but a cached build exists."
            $r = Read-Host "Rebuild from scratch to apply them? [Y/n]"
            if ($r -eq "" -or $r -eq "y" -or $r -eq "Y") {
                Write-Host "--- Wiping build dir..."
                Remove-Item -Recurse -Force $macBuildPath -ErrorAction SilentlyContinue
                $cacheExists = $false
            }
            Write-Host ""
        }

        if (-not $cacheExists) {
            Write-Host "--- Configuring ($presetName)..."
            Write-Host "    cmake --preset $presetName$(if ($ExtraFlags) {" $ExtraFlags"})"
            Write-Host ""
            Push-Location $WinSrcPath
            $cfgArgs = @("--preset", $presetName)
            if ($ExtraFlags -ne "") { $cfgArgs += $ExtraFlags.Split(" ", [System.StringSplitOptions]::RemoveEmptyEntries) }
            & $cmakeExe @cfgArgs
            $cfgResult = $LASTEXITCODE
            Pop-Location
            if ($cfgResult -ne 0) { Write-Error "cmake configure failed."; exit 1 }
            Write-Host ""
        } else {
            Write-Host "--- Skipping configure (cache exists). Choose Rebuild to reconfigure."
            Write-Host ""
        }

        Write-Host "--- Building $selectedTarget..."
        & $cmakeExe --build $macBuildPath --target $selectedTarget
        if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit 1 }
        Write-Host ""
        Write-Host "==> Build complete."
        Write-Host "==> Binary: $binaryPath"
        Write-Host ""
        $r = Read-Host "Run $selectedTarget now? [Y/n]"
        if ($r -eq "" -or $r -eq "y" -or $r -eq "Y") {
            if ($isTestTarget -and $RunArgs -eq "") {
                Write-Host ""
                $RunArgs = Read-Host "Test args (blank = run all, e.g. [map])"
            }
            $Action = "run"
        }
        Write-Host ""

    } else {
        # ── Linux (WSL) build ─────────────────────────────────────────────────

        if ($Action -eq "rebuild") {
            Write-Host "--- Wiping build dir..."
            wsl bash -c "rm -rf $wslBuildPath"
            if ($LASTEXITCODE -ne 0) { Write-Error "Could not wipe build dir."; exit 1 }
            Write-Host ""
        }

        # Derive required packages from the preset's resolved cacheVariables
        Write-Host "--- Checking WSL dependencies..."
        $pkgList    = (Get-RequiredPackages $cacheVars) -join " "
        $rawMissing = wsl bash -c "for p in $pkgList; do if ! dpkg -s `$p 2>/dev/null | grep -q 'install ok installed'; then printf '%s ' `$p; fi; done"
        $missing    = "$rawMissing".Trim()
        if ($missing -ne "") {
            Write-Host "--- Installing: $missing"
            wsl -u root bash -c "apt-get update -qq"
            wsl -u root bash -c "DEBIAN_FRONTEND=noninteractive apt-get install -y $missing"
            if ($LASTEXITCODE -ne 0) { Write-Error "Dependency install failed. Check the output above."; exit 1 }
            Write-Host "--- Dependencies installed."
        } else {
            Write-Host "--- All dependencies present."
        }
        Write-Host ""

        # TSan kernel fix: high ASLR entropy causes shadow memory conflicts
        if ($isTSan) {
            $rawBits  = wsl -u root bash -c "sysctl -n vm.mmap_rnd_bits 2>/dev/null"
            $mmapBits = "$rawBits".Trim()
            if ($mmapBits -ne "" -and [int]$mmapBits -gt 28) {
                Write-Host "--- Applying TSan kernel fix (vm.mmap_rnd_bits: $mmapBits -> 28)..."
                wsl -u root bash -c "sysctl -w vm.mmap_rnd_bits=28"
                wsl -u root bash -c "printf 'vm.mmap_rnd_bits=28\n' > /etc/sysctl.d/tsan.conf"
                Write-Host "--- Fix applied and made permanent via /etc/sysctl.d/tsan.conf."
                Write-Host ""
            }
        }

        # rsync
        Write-Host "--- Syncing source to WSL..."
        wsl rsync -a --delete `
            --exclude='.git/' `
            --exclude='out/' `
            --exclude='.vs/' `
            "$WslSrcPath/" "$WslSrcDir/"
        if ($LASTEXITCODE -ne 0) { Write-Error "rsync failed."; exit 1 }
        Write-Host "--- Sync complete."
        Write-Host ""

        # Write version.h from Windows git (WSL source has no .git; CMake skips
        # writing version.h when git fails, so ours persists through configure)
        $gitVer  = (git -C $WinSrcPath describe --tags --always --match "[0-9A-Z]*.[0-9A-Z]*" 2>$null)
        if (-not $gitVer) { $gitVer = "unknown" }
        $buildTs = Get-Date -Format "yyyy-MM-dd-HHmm"
        $vhStr   = "// NOLINT(cata-header-guard)`n#define VERSION `"$gitVer`"`n#define BUILD_TIMESTAMP `"$buildTs`"`n"
        $b64     = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($vhStr))
        wsl bash -c "echo '$b64' | base64 -d > $WslSrcDir/src/version.h"
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Failed to write version.h - version will show as HEAD-HASH (non-fatal)."
        } else {
            Write-Host "--- Version: $gitVer ($buildTs)"
        }
        Write-Host ""

        # cmake configure using the preset (skipped if cache already exists).
        # -B overrides the preset's binaryDir to keep builds in $WslBldDir.
        wsl bash -c "test -f $wslBuildPath/CMakeCache.txt"
        $cacheExists = ($LASTEXITCODE -eq 0)

        if ($cacheExists -and $ExtraFlags -ne "") {
            Write-Host "--- Extra flags only apply during configure, but a cached build exists."
            $r = Read-Host "Rebuild from scratch to apply them? [Y/n]"
            if ($r -eq "" -or $r -eq "y" -or $r -eq "Y") {
                Write-Host "--- Wiping build dir..."
                wsl bash -c "rm -rf $wslBuildPath"
                if ($LASTEXITCODE -ne 0) { Write-Error "Could not wipe build dir."; exit 1 }
                $cacheExists = $false
            }
            Write-Host ""
        }

        if (-not $cacheExists) {
            Write-Host "--- Configuring ($presetName)..."
            Write-Host "    cmake --preset $presetName -B $wslBuildPath$(if ($ExtraFlags) {" $ExtraFlags"})"
            Write-Host ""
            # cd to source so cmake --preset finds CMakePresets.json there.
            # Pipe through grep to suppress the expected "not a git repository" stderr from
            # CMake's version detection (harmless - we already wrote version.h above).
            # String concatenation keeps ${PIPESTATUS[0]} out of PS variable expansion.
            $configCmd = "cd $WslSrcDir; cmake --preset $presetName -B $wslBuildPath"
            if ($ExtraFlags -ne "") { $configCmd += " $ExtraFlags" }
            wsl bash -c ($configCmd + ' 2>&1 | grep -v "not a git repository"; exit ${PIPESTATUS[0]}')
            if ($LASTEXITCODE -ne 0) { Write-Error "cmake configure failed."; exit 1 }
            Write-Host ""
        } else {
            Write-Host "--- Skipping configure (cache exists). Choose Rebuild to reconfigure."
            Write-Host ""
        }

        Write-Host "--- Building $selectedTarget..."
        wsl bash -c "cmake --build $wslBuildPath --target $selectedTarget"
        if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit 1 }
        Write-Host ""
        Write-Host "==> Build complete."
        Write-Host ""
        $r = Read-Host "Run $selectedTarget now? [Y/n]"
        if ($r -eq "" -or $r -eq "y" -or $r -eq "Y") {
            if ($isTestTarget -and $RunArgs -eq "") {
                Write-Host ""
                $RunArgs = Read-Host "Test args (blank = run all, e.g. [map])"
            }
            $Action = "run"
        }
        Write-Host ""
    }
}

# ── Run ───────────────────────────────────────────────────────────────────────
if ($Action -eq "run") {
    if (-not $IsLinux -and -not (Test-Path $binaryPath)) {
        Write-Error "Binary not found: $binaryPath"
        Write-Error "Ensure the build succeeded, or run the 'Build' action first."
        exit 1
    }
    Write-Host "--- Running $selectedTarget $RunArgs ..."
    Write-Host ""
    if ($IsLinux) {
        wsl bash -c "cd $WslSrcDir; $binaryPath $RunArgs"
        $runExit = $LASTEXITCODE
    } else {
        # Windows and macOS: run from source root so the game/tests can locate data/ and gfx/
        Push-Location $WinSrcPath
        if ($RunArgs -ne "") { & $binaryPath $RunArgs.Split() } else { & $binaryPath }
        $runExit = $LASTEXITCODE
        Pop-Location
    }
    Write-Host ""
    if ($runExit -ne 0) { Write-Host "==> Process exited with code $runExit" }
    else { Write-Host "==> Done." }
}

# ── Save last config + loop prompt ────────────────────────────────────────────
if ($savedAction -ne "delete") {
    $lastConfig = @{
        Platform    = $Platform
        PresetName  = $presetName
        PresetLabel = $presetLabel
        BuildTypeIdx = if ($IsWin -and $selectedBuildType) {
                           ($WinBuildTypes.IndexOf($selectedBuildType) + 1).ToString()
                       } else { "" }
        BuildType   = if ($IsWin) { $selectedBuildType } else { "" }
        Target      = $selectedTarget
        Action      = $savedAction
        RunArgs     = $RunArgs
    }
}

Write-Host ""
Write-Host "==> What next?"
Write-Host "  Enter  Start fresh"
if ($lastConfig) {
    $rl = "$($lastConfig.Action): $($lastConfig.PresetLabel)"
    if ($lastConfig.BuildType) { $rl += " | $($lastConfig.BuildType)" }
    $rl += " | $($lastConfig.Target)"
    Write-Host "  l      Repeat last  [$rl]"
}
Write-Host "  x      Exit"
Write-Host ""
$next = (Read-Host "Choice").ToLower().Trim()

if ($next -eq "x") { exit 0 }
if ($next -eq "l" -and $lastConfig) {
    $Platform   = $lastConfig.Platform
    $Preset     = $lastConfig.PresetName
    $BuildType  = $lastConfig.BuildTypeIdx
    $Target     = $lastConfig.Target
    $Action     = $lastConfig.Action
    $RunArgs    = $lastConfig.RunArgs
    $ExtraFlags = ""
} else {
    $Platform   = $ParamPlatform
    $Preset     = $ParamPreset
    $BuildType  = $ParamBuildType
    $Target     = $ParamTarget
    $Action     = $ParamAction
    $RunArgs    = $ParamRunArgs
    $ExtraFlags = $ParamExtraFlags
}

} # end while ($true)
