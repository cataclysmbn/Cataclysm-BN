param(
    [string]$Version = $env:SLANG_VERSION,
    [string]$InstallDir = $env:SLANG_INSTALL_DIR,
    [string]$DownloadDir = $env:SLANG_DOWNLOAD_DIR
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = "v2026.10.2"
}
if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    $InstallDir = "build-data/slang/current"
}
if ([string]::IsNullOrWhiteSpace($DownloadDir)) {
    $DownloadDir = "build-data/slang/downloads"
}

$versionNumber = $Version.TrimStart("v")
$arch = switch ($env:PROCESSOR_ARCHITECTURE) {
    "AMD64" { "x86_64"; break }
    "ARM64" { "aarch64"; break }
    default { throw "Unsupported Slang host architecture: $env:PROCESSOR_ARCHITECTURE" }
}

$asset = "slang-$versionNumber-windows-$arch.zip"
$url = "https://github.com/shader-slang/slang/releases/download/$Version/$asset"
$archive = Join-Path $DownloadDir $asset
$tmpDir = Join-Path ([System.IO.Path]::GetTempPath()) ("cata-slang-" + [System.Guid]::NewGuid())

New-Item -ItemType Directory -Force -Path $DownloadDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $InstallDir) | Out-Null

try {
    if (!(Test-Path $archive)) {
        Write-Host "Downloading $asset"
        Invoke-WebRequest -Uri $url -OutFile $archive
    }

    New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null
    Expand-Archive -Path $archive -DestinationPath $tmpDir -Force

    $slangc = Get-ChildItem -Path $tmpDir -Filter slangc.exe -Recurse | Select-Object -First 1
    if ($null -eq $slangc) {
        throw "Downloaded Slang archive did not contain slangc.exe"
    }

    $extractedRoot = Split-Path -Parent (Split-Path -Parent $slangc.FullName)
    if (Test-Path $InstallDir) {
        Remove-Item -Recurse -Force $InstallDir
    }
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    Copy-Item -Path (Join-Path $extractedRoot "*") -Destination $InstallDir -Recurse -Force

    Write-Host "Slang installed to $InstallDir"
    Write-Host "Configure with: -DCATA_SLANG_ROOT=$InstallDir"
}
finally {
    if (Test-Path $tmpDir) {
        Remove-Item -Recurse -Force $tmpDir
    }
}
