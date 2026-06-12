param(
    [string]$SourceDir = "",
    [string]$OutputDir = "",
    [string]$CpuOutputDir = "",
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

function Get-CommandPath {
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

    throw "Could not find $Name. Set $EnvName or install the required build tool."
}

function Get-VcpkgShadercrossCandidates {
    $triplets = @("x64-windows-static", "x86-windows-static")
    $roots = @(
        (Join-Path $repoRoot "msvc-full-features\vcpkg_installed"),
        (Join-Path $repoRoot "vcpkg_installed")
    )
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

function Test-OutputsOutdated {
    param(
        [System.IO.FileInfo]$Source,
        [string[]]$Outputs
    )

    foreach ($output in $Outputs) {
        if (!(Test-Path $output)) {
            return $true
        }
        if ((Get-Item $output).LastWriteTimeUtc -lt $Source.LastWriteTimeUtc) {
            return $true
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
        [string]$OutputPath
    )

    & $Slangc $InputPath -entry main -stage compute -profile sm_6_0 -target hlsl -o $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "slangc HLSL generation failed for $InputPath"
    }
}

function Invoke-SlangCpu {
    param(
        [string]$Slangc,
        [string]$InputPath,
        [string]$OutputPath
    )

    & $Slangc $InputPath -entry cpu_main -stage compute -target cpp -DCATA_SLANG_CPU=1 -o $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "slangc CPU C++ generation failed for $InputPath"
    }
}

if (!(Test-Path $SourceDir)) {
    throw "Shader source directory not found: $SourceDir"
}

$hlslShaders = @(Get-ChildItem -Path $SourceDir -Filter "*.hlsl" -File)
$slangShaders = @(Get-ChildItem -Path $SourceDir -Filter "*.slang" -File)
if ($hlslShaders.Count -eq 0 -and $slangShaders.Count -eq 0) {
    throw "No HLSL or Slang shaders found in $SourceDir"
}

$shadercross = Get-CommandPath `
    -Name "shadercross.exe" `
    -EnvName "SHADERCROSS" `
    -Candidates (Get-VcpkgShadercrossCandidates)

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
    if (Test-OutputsOutdated -Source $shader -Outputs ($shaderOutputs + @($hlslOut))) {
        Write-Host "Compiling $($shader.Name) -> HLSL"
        Invoke-SlangHlsl $slangc $shader.FullName $hlslOut
        Invoke-Shadercross $shadercross $hlslOut "spirv" "compute" $shaderOutputs[0]
        Invoke-Shadercross $shadercross $hlslOut "msl" "compute" $shaderOutputs[1]
        Invoke-Shadercross $shadercross $hlslOut "dxil" "compute" $shaderOutputs[2]
    }

    if ($GenerateCpu) {
        $cpuOut = Join-Path $CpuOutputDir "$($shader.BaseName).cpp"
        if (Test-OutputsOutdated -Source $shader -Outputs @($cpuOut)) {
            Write-Host "Compiling $($shader.Name) -> CPU C++"
            Invoke-SlangCpu $slangc $shader.FullName $cpuOut
        }
    }
}
