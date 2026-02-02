#!/usr/bin/env pwsh
# FlatBuffers schema compilation script

# Configuration
$flatcPath = "flatc"
$schemasDir = ".\serialize\flatbuf\schemas"
$outputDir = ".\generated"

# Colors for output
$ErrorColor = "Red"
$SuccessColor = "Green"
$InfoColor = "Cyan"

# Check if flatc is available
try {
    $null = & $flatcPath --version 2>&1
} catch {
    Write-Host "Error: flatc compiler not found in PATH" -ForegroundColor $ErrorColor
    Write-Host "Please install FlatBuffers or add flatc to your PATH" -ForegroundColor $ErrorColor
    exit 1
}

# Check if schemas directory exists
if (-not (Test-Path $schemasDir)) {
    Write-Host "Error: Schemas directory not found: $schemasDir" -ForegroundColor $ErrorColor
    exit 1
}

# Create output directory if it doesn't exist
if (-not (Test-Path $outputDir)) {
    Write-Host "Creating output directory: $outputDir" -ForegroundColor $InfoColor
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

# Find all .fbs files recursively
$fbsFiles = Get-ChildItem -Path $schemasDir -Filter "*.fbs" -Recurse

if ($fbsFiles.Count -eq 0) {
    Write-Host "Warning: No .fbs files found in $schemasDir" -ForegroundColor Yellow
    exit 0
}

Write-Host "Found $($fbsFiles.Count) FlatBuffers schema file(s)" -ForegroundColor $InfoColor
Write-Host ""

# Compile each schema
$successCount = 0
$failCount = 0

foreach ($file in $fbsFiles) {
    $relativePath = $file.FullName.Substring((Get-Location).Path.Length + 1)
    Write-Host "Compiling: $relativePath" -ForegroundColor $InfoColor

    # Compile with C++ output
    # -I specifies include path for imports
    # -o specifies output directory
    # --cpp generates C++ code
    # --gen-mutable generates mutable FlatBuffers (optional)
    # --scoped-enums generates scoped enums (C++11)
    $result = & $flatcPath --cpp --scoped-enums -I $schemasDir -o $outputDir $file.FullName 2>&1

    if ($LASTEXITCODE -eq 0) {
        Write-Host "  [OK] Success" -ForegroundColor $SuccessColor
        $successCount++
    } else {
        Write-Host "  [FAIL] Failed" -ForegroundColor $ErrorColor
        Write-Host "  $result" -ForegroundColor $ErrorColor
        $failCount++
    }
}

Write-Host ""
$summaryColor = if ($failCount -eq 0) { $SuccessColor } else { "Yellow" }
Write-Host "Compilation complete: $successCount succeeded, $failCount failed" -ForegroundColor $summaryColor

if ($failCount -gt 0) {
    exit 1
}
