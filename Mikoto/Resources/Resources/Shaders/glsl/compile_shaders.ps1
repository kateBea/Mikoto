#!/usr/bin/env pwsh
# Requires: glslc in PATH

<#
 ---------------------------------------------------------------------------
 Vulkan GLSL -> SPIR-V shader compiler script (PowerShell edition)
 Automatically detects shader stage from filename suffix.
 ---------------------------------------------------------------------------
#>

$ErrorActionPreference = "Stop"

# Get directory of this script and switch to it
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $SCRIPT_DIR

# Shader stage mapping
$STAGE_MAP = @{
    "vert" = "vertex"
    "frag" = "fragment"
    "comp" = "compute"
}

Write-Host "Compiling shaders in: $SCRIPT_DIR"
Write-Host "------------------------------------------------------------"

# Find *.glsl files
$files = Get-ChildItem -Filter *.glsl -ErrorAction SilentlyContinue

if (-not $files) {
    Write-Host "No .glsl shaders found."
    return
}

foreach ($file in $files) {
    $base   = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)
    $output = "$base.sprv"
    $stage  = ""

    # Stage detection (suffix match)
    foreach ($key in $STAGE_MAP.Keys) {
        if ($base -match $key) {
            $stage = $STAGE_MAP[$key]
            break
        }
    }

    if (-not $stage) {
        Write-Host "Skipping $($file.Name) — unknown shader stage"
        continue
    }

    Write-Host "-> Compiling $($file.Name) → $output ($stage)"

    # Invoke glslc
    glslc -O "-fshader-stage=$stage" $file.FullName `
        -o $output --target-env="vulkan1.3" --target-spv="spv1.6" -g
}

Write-Host "------------------------------------------------------------"
Write-Host "Shader compilation complete."
