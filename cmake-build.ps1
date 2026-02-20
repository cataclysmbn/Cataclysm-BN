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
#   /c "$(SolutionDir)cmake-build.bat -Platform win -Preset 1 -BuildType 1 -Action debug"
#   /c "$(SolutionDir)cmake-build.bat -Platform linux -Preset linux-slim -Target cataclysm-bn-tiles -Action build"
#
# For the debug action, add a second External Tool entry (e.g. "BN Debug") using -Action debug.
# Windows: opens a new VS instance with devenv /debugexe (full debugger attached from start).
# Linux  : starts SSH in WSL, port-proxies localhost:2222 -> WSL:22, launches game in a new
#          WSL window; attach via Debug > Attach to Process (Connection type: SSH, target: localhost:2222).

param(
    [string]$Platform   = "",   # win | linux | mac
    [string]$Preset     = "",   # configure preset name or selection number
    [string]$BuildType  = "",   # Windows only: 1=Debug 2=RelWithDebInfo 3=Release
    [string]$Target     = "",   # cmake build target (derived from preset if blank)
    [string]$Action     = "",   # build | run | rebuild | delete | debug
    [string]$RunArgs    = "",   # forwarded verbatim to the binary when running
    [string]$ExtraFlags = "",   # extra cmake configure flags, e.g. -DFOO=ON
    [switch]$Gui                # force WinForms UI even in an interactive console (used by elevated WSL re-launch)
)

# ── User configuration ────────────────────────────────────────────────────────
$WslSrcDir     = "~/cbn"        # WSL path for synced source
$WslBldDir     = "~/cbn-build"  # WSL path for build dirs (overrides preset binaryDir)
$VcpkgRoot     = ""             # Windows only: path to vcpkg root (auto-detected if blank)
$WslSdlDisplay = 0              # WSL run: SDL monitor index (0=first, 1=second, -1=SDL default)

# ── Path resolution ───────────────────────────────────────────────────────────
$WinSrcPath = $PSScriptRoot
$DriveLetter = $WinSrcPath.Substring(0, 1).ToLower()
$WslSrcPath  = "/mnt/$DriveLetter$($WinSrcPath.Substring(2).Replace('\', '/'))"
$wslExe      = "$env:SystemRoot\System32\wsl.exe"
function wsl { & $wslExe @args }

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

# ── Interactivity helpers ──────────────────────────────────────────────────────
# When stdin is a real console (terminal), interactive text menus work normally.
# When redirected (e.g. VS "Use Output window"), we fall back to GUI pickers so
# the tool still works without a terminal window.
$isInteractive = (-not [Console]::IsInputRedirected) -and (-not $Gui)

# Show a numbered text menu (terminal) or an Out-GridView picker (GUI mode).
# $Items: ordered hashtable  Key => Label,
#         plain string array (Value = Label), or
#         @([PSCustomObject]@{Value=…; Label=…}) for pre-built entries.
# Returns the chosen Value; exits 0 if the user dismisses the picker.
function Select-Menu {
    param([string]$Title, $Items)

    # Normalise to @{Value; Label} entries.
    if ($Items -is [System.Collections.IDictionary]) {
        $entries = @($Items.GetEnumerator() | ForEach-Object {
            [PSCustomObject]@{ Value = $_.Key; Label = $_.Value }
        })
    } elseif ($Items.Count -gt 0 -and $Items[0] -is [string]) {
        $entries = @($Items | ForEach-Object { [PSCustomObject]@{ Value = $_; Label = $_ } })
    } else {
        $entries = @($Items)
    }

    if ($script:isInteractive) {
        Write-Host ""
        Write-Host "${Title}:"
        for ($i = 0; $i -lt $entries.Count; $i++) {
            Write-Host "  $($i + 1)  $($entries[$i].Label)"
        }
        Write-Host ""
        $idx = [int](Read-Host "Enter number") - 1
        if ($idx -lt 0 -or $idx -ge $entries.Count) { Write-Error "Invalid selection."; exit 1 }
        return $entries[$idx].Value
    } else {
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing

        $labels  = @($entries | ForEach-Object { $_.Label })

        # Size the window to fit the longest label (rough 7-px-per-char estimate for 10pt Segoe UI).
        $maxChar = ($labels | ForEach-Object { $_.Length } | Measure-Object -Maximum).Maximum
        $fw = [Math]::Max(280, [Math]::Min(700, $maxChar * 7 + 60))
        $fh = [Math]::Max(120, [Math]::Min(500, $labels.Count * 22 + 80))

        $form = New-Object System.Windows.Forms.Form
        $form.Text            = $Title
        $form.ClientSize      = New-Object System.Drawing.Size($fw, $fh)
        $form.StartPosition   = "CenterScreen"
        $form.FormBorderStyle = "Sizable"
        $form.MaximizeBox     = $false
        $form.MinimizeBox     = $false
        $form.TopMost         = $true
        $form.Font            = New-Object System.Drawing.Font("Segoe UI", 10)

        $list = New-Object System.Windows.Forms.ListBox
        $list.SetBounds(0, 0, $fw, $fh - 44)
        $list.Anchor        = "Top, Left, Right, Bottom"
        $list.BorderStyle   = "None"
        $list.IntegralHeight = $false
        foreach ($lbl in $labels) { [void]$list.Items.Add($lbl) }
        $list.SelectedIndex = 0

        $ok = New-Object System.Windows.Forms.Button
        $ok.Text         = "OK"
        $ok.SetBounds(($fw - 80) / 2, $fh - 38, 80, 28)
        $ok.Anchor       = "Bottom"
        $ok.DialogResult = [System.Windows.Forms.DialogResult]::OK

        $list.add_DoubleClick({ $form.DialogResult = [System.Windows.Forms.DialogResult]::OK })

        $form.Controls.Add($list)
        $form.Controls.Add($ok)
        $form.AcceptButton = $ok

        $dlg = $form.ShowDialog()
        $si  = $list.SelectedIndex
        $form.Dispose()

        if ($dlg -ne [System.Windows.Forms.DialogResult]::OK -or $si -lt 0) { exit 0 }
        return $entries[$si].Value
    }
}

# Prompt for a free-form text value.
# Uses a GUI InputBox when stdin is redirected; Read-Host otherwise.
function Read-Input {
    param([string]$Prompt, [string]$Default = "")
    if ($script:isInteractive) {
        return (Read-Host $Prompt)
    } else {
        Add-Type -AssemblyName Microsoft.VisualBasic
        return [Microsoft.VisualBasic.Interaction]::InputBox($Prompt, "cmake-build", $Default)
    }
}

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
    $Platform = Select-Menu "Select platform" $platformList
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
    if (-not (Test-Path $wslExe)) {
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
    $presetMap = [ordered]@{}
    foreach ($p in $availPresets) {
        $presetMap[$p.name] = if ($p.displayName) { $p.displayName } else { $p.name }
    }
    $chosenPresetName = Select-Menu "Select configure preset" $presetMap
    $selectedPreset   = $availPresets | Where-Object { $_.name -eq $chosenPresetName } | Select-Object -First 1
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
        $selectedBuildType = Select-Menu "Select build type" $WinBuildTypes
    }
}

# ── Target selection ──────────────────────────────────────────────────────────
$inferredTargets = @(Get-PresetTargets $cacheVars)
$selectedTarget  = $null

if ($Target -ne "") {
    $selectedTarget = $Target
} elseif ($inferredTargets.Count -eq 1) {
    $selectedTarget = $inferredTargets[0]
} else {
    $targetOptions  = $inferredTargets + @("(Enter a custom target name)")
    $pickedTarget   = Select-Menu "Select target" $targetOptions
    if ($pickedTarget -eq "(Enter a custom target name)") {
        $selectedTarget = Read-Input "Target name"
    } else {
        $selectedTarget = $pickedTarget
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
$validActions = @("build", "run", "rebuild", "delete", "debug")
if ($Action -ne "" -and $validActions -notcontains $Action) {
    Write-Error "Invalid -Action '$Action'. Valid: $($validActions -join ', ')."
    exit 1
}
if ($Action -eq "") {
    $actionItems = [ordered]@{
        "build"   = "Build"
        "run"     = "Run"
        "rebuild" = "Rebuild  (wipe build dir, reconfigure, build)"
        "delete"  = "Delete build"
        "debug"   = "Debug    (run under VS debugger / SSH attach)"
        "exit"    = "Exit"
    }
    $Action = Select-Menu "Select action" $actionItems
    if ($Action -eq "exit") { exit 0 }
}

if ($isTestTarget -and $Action -eq "run" -and $RunArgs -eq "" -and -not $PSBoundParameters.ContainsKey('RunArgs')) {
    Write-Host ""
    $RunArgs = Read-Input "Test args (blank = run all, e.g. [map])"
}

if (($Action -eq "build" -or $Action -eq "rebuild") -and $ExtraFlags -eq "" -and -not $PSBoundParameters.ContainsKey('ExtraFlags')) {
    Write-Host ""
    $ExtraFlags = Read-Input "Extra cmake flags (blank = none, e.g. -DFOO=ON)"
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

# ── Elevation check (Linux/WSL) ───────────────────────────────────────────────
# WSL requires admin. If not already elevated, re-launch as administrator.
# The elevated window is a real console with stdin, so we do NOT pass -Gui:
# text-based Read-Host prompts appear inline in the same console as build output,
# which is more visible than a WinForms popup that can end up behind other windows.
if ($IsLinux) {
    $isAdmin = (New-Object Security.Principal.WindowsPrincipal(
        [Security.Principal.WindowsIdentity]::GetCurrent()
    )).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        Write-Host "==> WSL builds require elevation. Relaunching as administrator..."
        $elevCmd  = "& '" + $PSCommandPath.Replace("'", "''")    + "'"
        $elevCmd += " -Platform linux"
        $elevCmd += " -Preset '"  + $presetName.Replace("'", "''")     + "'"
        $elevCmd += " -Target '"  + $selectedTarget.Replace("'", "''") + "'"
        $elevCmd += " -Action '"  + $Action.Replace("'", "''")         + "'"
        $elevCmd += " -RunArgs '"    + $RunArgs.Replace("'", "''")    + "'"
        $elevCmd += " -ExtraFlags '" + $ExtraFlags.Replace("'", "''") + "'"
        $encoded = [Convert]::ToBase64String([System.Text.Encoding]::Unicode.GetBytes($elevCmd))
        Start-Process powershell.exe -Verb RunAs -ArgumentList "-Sta -NoExit -ExecutionPolicy Bypass -EncodedCommand $encoded"
        exit 0
    }
}

# ── Delete ────────────────────────────────────────────────────────────────────
if ($Action -eq "delete") {
    $confirm = Select-Menu "Delete $buildDir?" ([ordered]@{ "yes" = "Yes - delete"; "no" = "No - cancel" })
    if ($confirm -ne "yes") { Write-Host "Cancelled."; continue }
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
            $r = Select-Menu "Rebuild from scratch to apply extra flags?" ([ordered]@{ "yes" = "Yes - wipe and rebuild"; "no" = "No - keep cache" })
            if ($r -eq "yes") {
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

        Write-Host "--- Building $selectedTarget ($selectedBuildType) ..."
        Write-Host "    (compile in progress - this will take a while on first build)"
        & $cmakeExe --build $winBuildDir --config $selectedBuildType --target $selectedTarget
        if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit 1 }
        Write-Host ""
        Write-Host "==> Build complete."
        Write-Host "==> Binary: $binaryPath"
        Write-Host ""
        $r = Select-Menu "Run $selectedTarget now?" ([ordered]@{
            "run"   = "Run"
            "debug" = "Debug    (attach VS debugger / SSH attach)"
            "no"    = "No"
        })
        if ($r -ne "no") {
            if ($isTestTarget -and $RunArgs -eq "") {
                Write-Host ""
                $RunArgs = Read-Input "Test args (blank = run all, e.g. [map])"
            }
            $Action = $r
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
            $r = Select-Menu "Rebuild from scratch to apply extra flags?" ([ordered]@{ "yes" = "Yes - wipe and rebuild"; "no" = "No - keep cache" })
            if ($r -eq "yes") {
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
        $r = Select-Menu "Run $selectedTarget now?" ([ordered]@{
            "run"   = "Run"
            "debug" = "Debug    (attach VS debugger / SSH attach)"
            "no"    = "No"
        })
        if ($r -ne "no") {
            if ($isTestTarget -and $RunArgs -eq "") {
                Write-Host ""
                $RunArgs = Read-Input "Test args (blank = run all, e.g. [map])"
            }
            $Action = $r
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

        # Ubuntu installs versioned LLVM/clang binaries (e.g. clang-14) but doesn't
        # always create bare-name alternatives. This block ensures tools are available,
        # auto-installing missing packages if needed, then resolves full paths for cmake.
        # Scripts are base64-encoded to avoid PowerShell 5.1 argument-quoting bugs.
        $resolvedCC     = ""
        $resolvedCXX    = ""
        $resolvedAR     = ""
        $resolvedRANLIB = ""
        $needsLlvmFix = ("$($cacheVars['CMAKE_C_COMPILER'])"  -match "clang") -or
                        ("$($cacheVars['CMAKE_CXX_COMPILER'])" -match "clang") -or
                        ("$($cacheVars['CMAKE_AR'])"           -match "llvm")
        if ($needsLlvmFix) {
            Write-Host "--- Setting up LLVM toolchain..."
            # Single root script: installs if missing, creates alternatives, prints TOOL_KEY=path.
            $llvmScript = @'
need_install() {
    local t=$1
    command -v "$t" >/dev/null 2>&1 && return 1
    ls /usr/bin/$t-[0-9]* >/dev/null 2>&1 && return 1
    ls /usr/lib/llvm-*/bin/$t >/dev/null 2>&1 && return 1
    return 0
}
resolve() {
    local p
    p=$(command -v "$1" 2>/dev/null)
    [ -z "$p" ] && p=$(ls /usr/bin/$1-[0-9]* 2>/dev/null | sort -V | tail -1)
    [ -z "$p" ] && p=$(ls /usr/lib/llvm-*/bin/$1 2>/dev/null | sort -V | tail -1)
    printf '%s' "$p"
}
if need_install clang; then
    echo '  Installing clang...'
    pkg=$(apt-cache search '^clang-[0-9]' 2>/dev/null | awk '{print $1}' | grep -xE 'clang-[0-9]+' | sort -V | tail -1)
    [ -z "$pkg" ] && pkg=clang
    DEBIAN_FRONTEND=noninteractive apt-get install -y -q "$pkg" >/dev/null
fi
if need_install llvm-ar; then
    echo '  Installing llvm...'
    pkg=$(apt-cache search '^llvm-[0-9]' 2>/dev/null | awk '{print $1}' | grep -xE 'llvm-[0-9]+' | sort -V | tail -1)
    [ -z "$pkg" ] && pkg=llvm
    DEBIAN_FRONTEND=noninteractive apt-get install -y -q "$pkg" >/dev/null
fi
for t in clang clang++ llvm-ar llvm-ranlib; do
    if ! command -v "$t" >/dev/null 2>&1; then
        b=$(ls /usr/bin/$t-[0-9]* 2>/dev/null | sort -V | tail -1)
        [ -z "$b" ] && b=$(ls /usr/lib/llvm-*/bin/$t 2>/dev/null | sort -V | tail -1)
        [ -n "$b" ] && update-alternatives --install /usr/bin/$t "$t" "$b" 100 2>/dev/null
    fi
done
for item in CC:clang CXX:clang++ AR:llvm-ar RANLIB:llvm-ranlib; do
    key=${item%%:*}
    tool=${item##*:}
    path=$(resolve "$tool")
    printf 'TOOL_%s=%s\n' "$key" "$path"
done
'@
            $llvmB64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($llvmScript))
            $toolPaths = @{}
            wsl -u root bash -c "echo $llvmB64 | base64 -d | bash" | ForEach-Object {
                $line = ($_ -replace "`r", "")
                if ($line -match '^TOOL_(\w+)=(.+)$') {
                    $toolPaths[$Matches[1]] = $Matches[2].Trim()
                } else {
                    Write-Host $line
                }
            }
            $resolvedCC     = if ($toolPaths['CC'])     { $toolPaths['CC'] }     else { '' }
            $resolvedCXX    = if ($toolPaths['CXX'])    { $toolPaths['CXX'] }    else { '' }
            $resolvedAR     = if ($toolPaths['AR'])     { $toolPaths['AR'] }     else { '' }
            $resolvedRANLIB = if ($toolPaths['RANLIB']) { $toolPaths['RANLIB'] } else { '' }
            Write-Host "  clang      : $(if ($resolvedCC)     { $resolvedCC }     else { 'NOT FOUND' })"
            Write-Host "  clang++    : $(if ($resolvedCXX)    { $resolvedCXX }    else { 'NOT FOUND' })"
            Write-Host "  llvm-ar    : $(if ($resolvedAR)     { $resolvedAR }     else { 'NOT FOUND' })"
            Write-Host "  llvm-ranlib: $(if ($resolvedRANLIB) { $resolvedRANLIB } else { 'NOT FOUND' })"
            if (-not $resolvedCC) {
                Write-Warning "clang not found after auto-install - cmake configure may fail"
            }
        }
        Write-Host ""

        # Verify ccache and mold are actually present as binaries (dpkg status alone can be stale).
        # If missing, install them and detect their full paths for cmake -D overrides.
        $resolvedCcache = ""
        $resolvedMold   = ""
        $needsCcache = ("$($cacheVars['CMAKE_C_COMPILER_LAUNCHER'])" -match "ccache") -or
                       ("$($cacheVars['CMAKE_CXX_COMPILER_LAUNCHER'])" -match "ccache")
        $needsMold   = ("$($cacheVars['LINKER'])" -match "mold")
        if ($needsCcache -or $needsMold) {
            Write-Host "--- Checking launcher/linker tools..."
        }
        if ($needsCcache) {
            $resolvedCcache = (wsl bash -c "command -v ccache 2>/dev/null").Trim() -replace "`r",""
            if (-not $resolvedCcache) {
                Write-Host "  Installing ccache..."
                wsl -u root bash -c "DEBIAN_FRONTEND=noninteractive apt-get install -y -q ccache >/dev/null"
                $resolvedCcache = (wsl bash -c "command -v ccache 2>/dev/null").Trim() -replace "`r",""
            }
            Write-Host "  ccache: $(if ($resolvedCcache) { $resolvedCcache } else { 'NOT FOUND - will disable' })"
        }
        if ($needsMold) {
            $resolvedMold = (wsl bash -c "command -v mold 2>/dev/null").Trim() -replace "`r",""
            if (-not $resolvedMold) {
                Write-Host "  Installing mold..."
                wsl -u root bash -c "DEBIAN_FRONTEND=noninteractive apt-get install -y -q mold >/dev/null"
                $resolvedMold = (wsl bash -c "command -v mold 2>/dev/null").Trim() -replace "`r",""
            }
            Write-Host "  mold  : $(if ($resolvedMold) { $resolvedMold } else { 'NOT FOUND' })"
        }
        if ($needsCcache -or $needsMold) { Write-Host "" }

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
        # NOTE: CMakeCache.txt is created even on FAILED configures (before compiler detection),
        # so we check for build.ninja/Makefile — only written on successful configure.
        wsl bash -c "test -f $wslBuildPath/build.ninja -o -f $wslBuildPath/Makefile"
        $cacheExists = ($LASTEXITCODE -eq 0)

        if ($cacheExists -and $ExtraFlags -ne "") {
            Write-Host "--- Extra flags only apply during configure, but a cached build exists."
            $r = Select-Menu "Rebuild from scratch to apply extra flags?" ([ordered]@{ "yes" = "Yes - wipe and rebuild"; "no" = "No - keep cache" })
            if ($r -eq "yes") {
                Write-Host "--- Wiping build dir..."
                wsl bash -c "rm -rf $wslBuildPath"
                if ($LASTEXITCODE -ne 0) { Write-Error "Could not wipe build dir."; exit 1 }
                $cacheExists = $false
            }
            Write-Host ""
        }

        if (-not $cacheExists) {
            # Wipe any stale CMakeCache.txt left by a previous failed configure.
            # CMakeCache.txt is written before compiler detection, so it exists even after failure.
            # A stale cache can cause cmake to use wrong (previously cached) compiler paths.
            wsl bash -c "rm -f $wslBuildPath/CMakeCache.txt; exit 0"
            Write-Host "--- Configuring ($presetName)..."
            Write-Host "    cmake --preset $presetName -B $wslBuildPath$(if ($ExtraFlags) {" $ExtraFlags"})"
            Write-Host ""
            # cd to source so cmake --preset finds CMakePresets.json there.
            # Run cmake directly (no grep pipe) so $LASTEXITCODE reliably reflects cmake's exit code.
            # (The pipe+PIPESTATUS approach was unreliable: cmake's non-zero exit propagated as 0.)
            $configCmd = "cd $WslSrcDir; cmake --preset $presetName -B $wslBuildPath"
            # Pass resolved full paths via -D to override the preset's bare tool names.
            # This ensures cmake finds the tools even if update-alternatives hasn't registered them.
            if ($resolvedCC  -ne "") { $configCmd += " -DCMAKE_C_COMPILER=$resolvedCC -DCMAKE_CXX_COMPILER=$resolvedCXX" }
            if ($resolvedAR  -ne "") { $configCmd += " -DCMAKE_AR=$resolvedAR -DCMAKE_RANLIB=$resolvedRANLIB" }
            if ($needsCcache) {
                if ($resolvedCcache) {
                    $configCmd += " -DCMAKE_C_COMPILER_LAUNCHER=$resolvedCcache -DCMAKE_CXX_COMPILER_LAUNCHER=$resolvedCcache"
                } else {
                    $configCmd += " -DCMAKE_C_COMPILER_LAUNCHER= -DCMAKE_CXX_COMPILER_LAUNCHER="
                }
            }
            if ($ExtraFlags -ne "") { $configCmd += " $ExtraFlags" }
            wsl bash -c $configCmd
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
        $r = Select-Menu "Run $selectedTarget now?" ([ordered]@{
            "run"   = "Run"
            "debug" = "Debug    (attach VS debugger / SSH attach)"
            "no"    = "No"
        })
        if ($r -ne "no") {
            if ($isTestTarget -and $RunArgs -eq "") {
                Write-Host ""
                $RunArgs = Read-Input "Test args (blank = run all, e.g. [map])"
            }
            $Action = $r
        }
        Write-Host ""
    }
}

# ── Run / Debug ───────────────────────────────────────────────────────────────
if ($Action -eq "run" -or $Action -eq "debug") {
    $isDebugRun = ($Action -eq "debug")

    if (-not $IsLinux -and -not (Test-Path $binaryPath)) {
        Write-Error "Binary not found: $binaryPath"
        Write-Error "Ensure the build succeeded, or run the 'Build' action first."
        exit 1
    }
    if ($isDebugRun) {
        Write-Host "--- Launching $selectedTarget under debugger..."
    } else {
        Write-Host "--- Running $selectedTarget $RunArgs ..."
    }
    Write-Host ""

    if ($IsLinux) {
        # SDL_VIDEO_DISPLAY selects which monitor the window opens on (0-indexed, -1 = SDL default).
        $sdlDisplay = if ($WslSdlDisplay -ge 0) { "SDL_VIDEO_DISPLAY=$WslSdlDisplay " } else { "" }
        # Verify the binary exists in WSL before run/debug.
        $binExists = (wsl bash -c "test -f $binaryPath && echo 1 || echo 0").Trim() -replace "`r",""
        if ($binExists -ne "1") {
            Write-Error "Linux binary not found in WSL: $binaryPath"
            Write-Error "Run the Build action first, or pass -Action build."
            exit 1
        }
        if ($isDebugRun) {
            # ── SSH server setup in WSL ───────────────────────────────────────────────
            # Build script as an array to avoid here-string column-0 requirements.
            Write-Host "--- Setting up SSH server for VS remote attach..."
            $sshScript = @(
                'command -v sshd >/dev/null 2>&1 || {'
                "    echo '  Installing openssh-server...'"
                '    DEBIAN_FRONTEND=noninteractive apt-get install -y -q openssh-server >/dev/null'
                '}'
                'cfg=/etc/ssh/sshd_config'
                'grep -qE ''^PasswordAuthentication yes'' "$cfg" 2>/dev/null || \'
                '    sed -i ''s/^#*[[:space:]]*PasswordAuthentication.*/PasswordAuthentication yes/'' "$cfg"'
                'ssh-keygen -A >/dev/null 2>&1'
                'service ssh start 2>&1 || service ssh restart 2>&1'
                'ss -tlnp 2>/dev/null | grep -q '':22'' && echo SSH_STATUS=ok || echo SSH_STATUS=failed'
                '# Allow GDB to attach to processes not in its process tree (ptrace scope 1 blocks SSH-attached GDB).'
                'echo 0 > /proc/sys/kernel/yama/ptrace_scope'
            ) -join "`n"
            $sshB64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($sshScript))
            $sshStatus = ""
            wsl -u root bash -c "echo $sshB64 | base64 -d | bash" | ForEach-Object {
                $line = ($_ -replace "`r", "")
                if ($line -match '^SSH_STATUS=(.+)$') { $sshStatus = $Matches[1] }
                else { Write-Host $line }
            }
            if ($sshStatus -ne "ok") {
                Write-Error "SSH server failed to start."
                exit 1
            }
            # ── Port proxy: Windows localhost:2222 → WSL IP:22 ───────────────────────
            # WSL2's IP changes on each WSL restart, so we refresh the proxy every run.
            $wslIp   = ((wsl hostname -I) -replace "`r","").Trim() -split '\s+' | Select-Object -First 1
            $wslUser = (wsl whoami).Trim() -replace "`r",""
            Write-Host "    WSL IP : $wslIp"
            netsh interface portproxy delete v4tov4 listenport=2222 listenaddress=127.0.0.1 2>&1 | Out-Null
            $netshOut = netsh interface portproxy add v4tov4 listenport=2222 listenaddress=127.0.0.1 `
                connectport=22 connectaddress=$wslIp 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Error "Port proxy setup failed: $netshOut"
                exit 1
            }
            $reach = Test-NetConnection -ComputerName 127.0.0.1 -Port 2222 -WarningAction SilentlyContinue
            if (-not $reach.TcpTestSucceeded) {
                Write-Error "localhost:2222 is not reachable after port proxy setup."
                Write-Error "SSH may not be listening in WSL, or the proxy is blocked."
                exit 1
            }
            Write-Host "    SSH    : reachable at localhost:2222"
            # ── Launch game in a new WSL console window ───────────────────────────────
            # Write the game script to a WSL temp file and open it via wsl.exe so it
            # gets a real TTY and WSLg environment (DISPLAY/WAYLAND_DISPLAY are set).
            $tmpScript   = "/tmp/cbn_run_${PID}.sh"
            $gameScript  = "cd $WslSrcDir" + [char]10
            $gameScript += "${sdlDisplay}${binaryPath} $RunArgs" + [char]10
            $gameScript += "rm -f $tmpScript"
            $gameB64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($gameScript))
            wsl bash -c "echo $gameB64 | base64 -d > $tmpScript && chmod +x $tmpScript"
            Start-Process wsl.exe -ArgumentList @("--", "bash", $tmpScript)
            Write-Host ""
            Write-Host "==> Game launched. Attach in Visual Studio:"
            Write-Host "    1. Debug > Attach to Process  (Ctrl+Alt+P)"
            Write-Host "    2. Connection type  : SSH"
            Write-Host "    3. Connection target: localhost:2222"
            Write-Host "       Username         : $wslUser"
            Write-Host "       (authenticate with your WSL password)"
            Write-Host "    4. Find '$selectedTarget' in the process list > Attach"
            Write-Host ""
            Write-Host "    VS saves the SSH connection - you only configure it once."
            Write-Host ""
            $runExit = 0
        } else {
            wsl bash -c "cd $WslSrcDir; ${sdlDisplay}${binaryPath} $RunArgs"
            $runExit = $LASTEXITCODE
        }
    } elseif ($isDebugRun -and $IsWin) {
        # cmake-build runs at standard (unelevated) integrity — same as VS — so COM ROT access
        # works directly. Never elevate cmake-build; the ROT is partitioned by integrity level.
        #
        # Derive DTE ProgIDs from every installed VS version (2022=17, 2026=18, ...).
        # GetActiveObject only returns running instances, so whichever VS the user has open
        # is found automatically — not the "latest installed" version.
        $vsWhere    = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        $dteProgIds = @()
        if (Test-Path $vsWhere) {
            foreach ($ver in @(& $vsWhere -all -prerelease -property installationVersion 2>$null)) {
                if ($ver -match '^(\d+)\.') {
                    $id = "VisualStudio.DTE.$($Matches[1]).0"
                    if ($dteProgIds -notcontains $id) { $dteProgIds += $id }
                }
            }
        }
        if (-not $dteProgIds) { $dteProgIds = @("VisualStudio.DTE.18.0", "VisualStudio.DTE.17.0", "VisualStudio.DTE.16.0") }

        $startParams = @{ FilePath = $binaryPath; WorkingDirectory = $WinSrcPath; PassThru = $true }
        if ($RunArgs -ne "") { $startParams['ArgumentList'] = $RunArgs.Split() }
        $gameProc = Start-Process @startParams
        Write-Host "==> Started $selectedTarget (PID $($gameProc.Id))"

        # Brief pause to let the process initialise; detect immediate crashes.
        Start-Sleep -Milliseconds 500
        if ($gameProc.HasExited) {
            Write-Host "==> ERROR: $selectedTarget exited immediately (code $($gameProc.ExitCode))."
            Write-Host "    Check that the binary and working directory are correct."
            $runExit = $gameProc.ExitCode
        } else {
            $attached   = $false
            $vsFound    = $false
            $attachErr  = ""
            $deadline   = (Get-Date).AddSeconds(5)
            do {
                foreach ($progId in $dteProgIds) {
                    try {
                        $dte     = [Runtime.InteropServices.Marshal]::GetActiveObject($progId)
                        $vsFound    = $true
                        $targetProc = $null
                        foreach ($p in $dte.Debugger.LocalProcesses) {
                            if ([int]$p.ProcessID -eq $gameProc.Id) { $targetProc = $p; break }
                        }
                        if ($targetProc) {
                            try   { [void]$targetProc.Attach(); $attached = $true; break }
                            catch { $attachErr = "$_" }
                        }
                    } catch { }
                }
                if (-not $attached) { Start-Sleep -Milliseconds 200 }
            } while (-not $attached -and (Get-Date) -lt $deadline)

            if ($attached) {
                Write-Host "==> VS debugger attached."
            } elseif ($vsFound) {
                Write-Host "==> VS found but could not attach to PID $($gameProc.Id)."
                if ($attachErr) {
                    Write-Host "    Attach() error: $attachErr"
                } else {
                    Write-Host "    Process not found in VS's list after 5 s."
                    Write-Host "    If VS is running elevated, restart it without elevation."
                }
                Write-Host "    Attach manually: Debug > Attach to Process."
            } else {
                Write-Host "==> No running VS instance found."
                Write-Host "    Attach manually: Debug > Attach to Process > PID $($gameProc.Id)."
            }
$gameProc.WaitForExit()
            $runExit = $gameProc.ExitCode
        }
    } else {
        # Normal run - Windows, macOS, or debug on macOS (falls back to plain run).
        # Run from source root so the game/tests can locate data/ and gfx/.
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

$nextItems = [ordered]@{ "new" = "Start fresh" }
if ($lastConfig) {
    $rl = "$($lastConfig.Action): $($lastConfig.PresetLabel)"
    if ($lastConfig.BuildType) { $rl += " | $($lastConfig.BuildType)" }
    $rl += " | $($lastConfig.Target)"
    $nextItems["last"] = "Repeat last  [$rl]"
}
$nextItems["exit"] = "Exit"
$next = Select-Menu "What next?" $nextItems

if ($next -eq "exit") { exit 0 }
if ($next -eq "last" -and $lastConfig) {
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
