# HarmonyOS Shader Compilation Script
# Compiles GLSL shaders to SPIR-V and copies to rawfile directory

param(
    [string]$DevEcoPath = "D:\Program Files\Huawei\DevEco Studio",
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Paths
$ProjectRoot = $PSScriptRoot
$ShaderDir = Join-Path $ProjectRoot "entry\src\main\cpp\render\agenui_engine\shaders"
$RawfileDir = Join-Path $ProjectRoot "entry\src\main\resources\rawfile"
$CompilerPath = Join-Path $DevEcoPath "sdk\default\openharmony\toolchains\glslang_validator.exe"

# Check compiler exists
if (-not (Test-Path $CompilerPath)) {
    Write-Error "Shader compiler not found at: $CompilerPath"
    Write-Host "Please install DevEco Studio or specify -DevEcoPath parameter"
    exit 1
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Compiling Shaders to SPIR-V" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Find all shader files
$Shaders = Get-ChildItem -Path $ShaderDir -Filter "*.vert" -File
$Shaders += Get-ChildItem -Path $ShaderDir -Filter "*.frag" -File

if ($Shaders.Count -eq 0) {
    Write-Warning "No shader files found in: $ShaderDir"
    exit 0
}

$CompiledCount = 0
$FailedCount = 0

foreach ($Shader in $Shaders) {
    $SpvFile = "$($Shader.Name).spv"
    $SpvPath = Join-Path $ShaderDir $SpvFile
    
    Write-Host "Compiling: $($Shader.Name)" -NoNewline
    
    $Output = & $CompilerPath -V $Shader.FullName -o $SpvPath 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host " [OK]" -ForegroundColor Green
        $CompiledCount++
        
        if ($Verbose) {
            Write-Host "  -> $SpvFile" -ForegroundColor DarkGray
        }
    } else {
        Write-Host " [FAILED]" -ForegroundColor Red
        Write-Host $Output
        $FailedCount++
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Copying to Rawfile Directory" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Copy all .spv files to rawfile directory
$SpvFiles = Get-ChildItem -Path $ShaderDir -Filter "*.spv" -File
$CopiedCount = 0

foreach ($Spv in $SpvFiles) {
    $DestPath = Join-Path $RawfileDir $Spv.Name
    
    Copy-Item $Spv.FullName $DestPath -Force
    Write-Host "Copied: $($Spv.Name)" -ForegroundColor Green
    $CopiedCount++
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Compiled: $CompiledCount shaders" -ForegroundColor Green
Write-Host "Failed:   $FailedCount shaders" -ForegroundColor $(if ($FailedCount -gt 0) { "Red" } else { "Green" })
Write-Host "Copied:   $CopiedCount SPV files to rawfile/" -ForegroundColor Green
Write-Host ""

if ($FailedCount -gt 0) {
    exit 1
}