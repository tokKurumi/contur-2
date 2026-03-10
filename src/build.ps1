# src/build.ps1 — convenience wrapper for CMake preset builds (Windows)
# Usage: powershell -File src/build.ps1 [-Preset debug] [-SourceDir src]

param(
    [ValidateSet("debug", "release")]
    [string]$Preset = "debug",

    [string]$SourceDir = $PSScriptRoot
)

$ErrorActionPreference = "Stop"

# On Windows, map preset names to win-* variants
$ActualPreset = "win-$Preset"

$ErrorActionPreference = "Stop"

# Resolve absolute source path
$SourceDir = (Resolve-Path $SourceDir).Path

if (-not (Test-Path "$SourceDir/CMakeLists.txt")) {
    Write-Error "[build] ERROR: CMakeLists.txt not found in $SourceDir"
    exit 1
}

Write-Host "[build] Configuring (preset=$ActualPreset)..."
cmake --preset $ActualPreset -S $SourceDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[build] Building..."
Push-Location $SourceDir
try {
    cmake --build --preset $ActualPreset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}

Write-Host "[build] Done."
