param(
    [string]$SourceDir = "",
    [string]$OutputDir = "",
    [string]$CpuOutputDir = "",
    [string]$VcpkgTriplet = "",
    [switch]$GenerateCpu
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = Join-Path $repoRoot "src\shaders"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "data\shaders"
}
if ([string]::IsNullOrWhiteSpace($CpuOutputDir)) {
    $CpuOutputDir = Join-Path $repoRoot "msvc-full-features\generated\slang_cpu"
}

function Find-CommandPath {
    param(
        [string]$Name,
        [string]$EnvName,
        [string[]]$Candidates
    )

    $envValue = [Environment]::GetEnvironmentVariable($EnvName)
    if (![string]::IsNullOrWhiteSpace($envValue) -and (Test-Path $envValue)) {
        return (Resolve-Path $envValue).Path
    }

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $cmd) {
        return $cmd.Source
    }

    foreach ($candidate in $Candidates) {
        if (![string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return ""
}

function Get-CommandPath {
    param(
        [string]$Name,
        [string]$EnvName,
        [string[]]$Candidates
    )

    $path = Find-CommandPath -Name $Name -EnvName $EnvName -Candidates $Candidates
    if (![string]::IsNullOrWhiteSpace($path)) {
        return $path
    }

    throw "Could not find $Name. Set $EnvName or install the required build tool."
}

function Get-VcpkgShadercrossCandidates {
    $triplets = @()
    if (![string]::IsNullOrWhiteSpace($VcpkgTriplet)) {
        $triplets += $VcpkgTriplet
    }
    foreach ($defaultTriplet in @("x64-windows-static", "x86-windows-static")) {
        if ($triplets -notcontains $defaultTriplet) {
            $triplets += $defaultTriplet
        }
    }
    $roots = @(
        (Join-Path $repoRoot "msvc-full-features\vcpkg_installed"),
        (Join-Path $repoRoot "vcpkg_installed"),
        (Join-Path $repoRoot "vcpkg\installed"),
        (Join-Path $repoRoot "..\vcpkg\installed"),
        "C:\vcpkg\installed"
    )
    if (![string]::IsNullOrWhiteSpace($env:USERPROFILE)) {
        $roots += Join-Path $env:USERPROFILE "vcpkg\installed"
    }
    foreach ($envName in @("VCPKG_ROOT", "VCPKG_INSTALLATION_ROOT")) {
        $envValue = [Environment]::GetEnvironmentVariable($envName)
        if (![string]::IsNullOrWhiteSpace($envValue)) {
            $roots += (Join-Path $envValue "installed")
        }
    }

    $candidates = @()
    foreach ($root in $roots) {
        foreach ($triplet in $triplets) {
            $candidates += Join-Path $root "$triplet\tools\sdl3-shadercross\shadercross.exe"
        }
    }
    return $candidates
}

function Get-VcpkgExecutableCandidates {
    $candidates = @()
    foreach ($envName in @("VCPKG_ROOT", "VCPKG_INSTALLATION_ROOT")) {
        $envValue = [Environment]::GetEnvironmentVariable($envName)
        if (![string]::IsNullOrWhiteSpace($envValue)) {
            $candidates += Join-Path $envValue "vcpkg.exe"
        }
    }

    $roots = @(
        (Join-Path $repoRoot "vcpkg"),
        (Join-Path $repoRoot "..\vcpkg"),
        "C:\vcpkg"
    )
    if (![string]::IsNullOrWhiteSpace($env:USERPROFILE)) {
        $roots += Join-Path $env:USERPROFILE "vcpkg"
    }

    foreach ($root in $roots) {
        $candidates += Join-Path $root "vcpkg.exe"
    }
    return $candidates
}

function Install-VcpkgShadercross {
    param(
        [string]$Triplet
    )

    if ([string]::IsNullOrWhiteSpace($Triplet)) {
        $Triplet = "x64-windows-static"
    }

    $vcpkg = Get-CommandPath `
        -Name "vcpkg.exe" `
        -EnvName "VCPKG_EXE" `
        -Candidates (Get-VcpkgExecutableCandidates)
    $manifestRoot = Join-Path $repoRoot "msvc-full-features"

    Write-Host "shadercross.exe not found; installing vcpkg manifest tools for $Triplet"
    Push-Location $manifestRoot
    try {
        & $vcpkg install --triplet $Triplet --clean-after-build
        if ($LASTEXITCODE -ne 0) {
            throw "vcpkg failed to install shadercross dependencies for $Triplet"
        }
    } finally {
        Pop-Location
    }
}

function Test-OutputsOutdated {
    param(
        [System.IO.FileInfo]$Source,
        [string[]]$Outputs,
        [System.IO.FileInfo[]]$AdditionalSources = @()
    )

    foreach ($output in $Outputs) {
        if (!(Test-Path $output)) {
            return $true
        }
        if ((Get-Item $output).LastWriteTimeUtc -lt $Source.LastWriteTimeUtc) {
            return $true
        }
        foreach ($additionalSource in $AdditionalSources) {
            if ((Get-Item $output).LastWriteTimeUtc -lt $additionalSource.LastWriteTimeUtc) {
                return $true
            }
        }
    }
    return $false
}

function Invoke-Shadercross {
    param(
        [string]$Shadercross,
        [string]$InputPath,
        [string]$Target,
        [string]$Stage,
        [string]$OutputPath
    )

    & $Shadercross $InputPath -s hlsl -d $Target -t $Stage -o $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "shadercross failed for $InputPath -> $OutputPath"
    }
}

function Invoke-SlangHlsl {
    param(
        [string]$Slangc,
        [string]$InputPath,
        [string]$IncludeDir,
        [string]$OutputPath
    )

    & $Slangc $InputPath -I $IncludeDir -entry main -stage compute -profile sm_6_0 -target hlsl -o $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "slangc HLSL generation failed for $InputPath"
    }
}

function Invoke-SlangCpu {
    param(
        [string]$Slangc,
        [string]$InputPath,
        [string]$IncludeDir,
        [string]$OutputPath
    )

    & $Slangc $InputPath -I $IncludeDir -entry cpu_main -stage compute -target cpp -DCATA_SLANG_CPU=1 -o $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "slangc CPU C++ generation failed for $InputPath"
    }
}

if (!(Test-Path $SourceDir)) {
    throw "Shader source directory not found: $SourceDir"
}

$hlslShaders = @(Get-ChildItem -Path $SourceDir -Filter "*.hlsl" -File)
$slangShaders = @(Get-ChildItem -Path $SourceDir -Filter "*.slang" -File)
$slangIncludes = @(Get-ChildItem -Path $SourceDir -Filter "*.slangh" -File)
if ($hlslShaders.Count -eq 0 -and $slangShaders.Count -eq 0) {
    throw "No HLSL or Slang shaders found in $SourceDir"
}

$shadercross = Find-CommandPath `
    -Name "shadercross.exe" `
    -EnvName "SHADERCROSS" `
    -Candidates (Get-VcpkgShadercrossCandidates)
if ([string]::IsNullOrWhiteSpace($shadercross)) {
    Install-VcpkgShadercross -Triplet $VcpkgTriplet
    $shadercross = Get-CommandPath `
        -Name "shadercross.exe" `
        -EnvName "SHADERCROSS" `
        -Candidates (Get-VcpkgShadercrossCandidates)
}

$slangCandidates = @(
    (Join-Path $repoRoot "build-data\slang\current\bin\slangc.exe")
)
$slangc = ""
if ($slangShaders.Count -ne 0 -or $GenerateCpu) {
    $localSlangc = $slangCandidates[0]
    $localSlangHeader = Join-Path $repoRoot "build-data\slang\current\include\slang-cpp-types.h"
    $envSlangc = [Environment]::GetEnvironmentVariable("SLANGC")
    if (!(Test-Path $localSlangHeader) -or
        ([string]::IsNullOrWhiteSpace($envSlangc) -and !(Test-Path $localSlangc))) {
        $installScript = Join-Path $repoRoot "build-scripts\install-slang.ps1"
        $installDir = Join-Path $repoRoot "build-data\slang\current"
        $downloadDir = Join-Path $repoRoot "build-data\slang\downloads"
        & powershell -NoProfile -ExecutionPolicy Bypass -File $installScript `
            -InstallDir $installDir `
            -DownloadDir $downloadDir
        if ($LASTEXITCODE -ne 0) {
            throw "Slang installation failed"
        }
    }
    if ([string]::IsNullOrWhiteSpace($envSlangc) -and (Test-Path $localSlangc)) {
        $slangc = (Resolve-Path $localSlangc).Path
    } else {
        $slangc = Get-CommandPath -Name "slangc.exe" -EnvName "SLANGC" -Candidates $slangCandidates
    }
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
if ($GenerateCpu) {
    New-Item -ItemType Directory -Force -Path $CpuOutputDir | Out-Null
}

$generatedHlslDir = Join-Path $repoRoot "build-data\generated\slang_hlsl"
if ($slangShaders.Count -ne 0) {
    New-Item -ItemType Directory -Force -Path $generatedHlslDir | Out-Null
}

$slangNames = @{}
foreach ($shader in $slangShaders) {
    $slangNames[$shader.BaseName] = $true
}

foreach ($shader in $hlslShaders) {
    if ($slangNames.ContainsKey($shader.BaseName)) {
        continue
    }

    $stage = "compute"
    if ($shader.BaseName.EndsWith("_vertex")) {
        $stage = "vertex"
    } elseif ($shader.BaseName.EndsWith("_fragment")) {
        $stage = "fragment"
    }

    $outputs = @(
        (Join-Path $OutputDir "$($shader.BaseName).spv"),
        (Join-Path $OutputDir "$($shader.BaseName).msl"),
        (Join-Path $OutputDir "$($shader.BaseName).dxil")
    )
    if (!(Test-OutputsOutdated -Source $shader -Outputs $outputs)) {
        continue
    }

    Write-Host "Compiling $($shader.Name)"
    Invoke-Shadercross $shadercross $shader.FullName "spirv" $stage $outputs[0]
    Invoke-Shadercross $shadercross $shader.FullName "msl" $stage $outputs[1]
    Invoke-Shadercross $shadercross $shader.FullName "dxil" $stage $outputs[2]
}

foreach ($shader in $slangShaders) {
    $hlslOut = Join-Path $generatedHlslDir "$($shader.BaseName).hlsl"
    $shaderOutputs = @(
        (Join-Path $OutputDir "$($shader.BaseName).spv"),
        (Join-Path $OutputDir "$($shader.BaseName).msl"),
        (Join-Path $OutputDir "$($shader.BaseName).dxil")
    )
    if (Test-OutputsOutdated -Source $shader -Outputs ($shaderOutputs + @($hlslOut)) `
            -AdditionalSources $slangIncludes) {
        Write-Host "Compiling $($shader.Name) -> HLSL"
        Invoke-SlangHlsl $slangc $shader.FullName $SourceDir $hlslOut
        Invoke-Shadercross $shadercross $hlslOut "spirv" "compute" $shaderOutputs[0]
        Invoke-Shadercross $shadercross $hlslOut "msl" "compute" $shaderOutputs[1]
        Invoke-Shadercross $shadercross $hlslOut "dxil" "compute" $shaderOutputs[2]
    }

    if ($GenerateCpu) {
        $cpuOut = Join-Path $CpuOutputDir "$($shader.BaseName).cpp"
        if (Test-OutputsOutdated -Source $shader -Outputs @($cpuOut) `
                -AdditionalSources $slangIncludes) {
            Write-Host "Compiling $($shader.Name) -> CPU C++"
            Invoke-SlangCpu $slangc $shader.FullName $SourceDir $cpuOut
        }
    }
}
