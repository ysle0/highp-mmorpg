# parse_compile_cfg.ps1
# Parses config.compile.toml and generates C++ constexpr struct header.
# TOML sections (depth <= 1) are converted to nested structs.

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

$TomlPath = Join-Path $ProjectRoot "network\config.compile.toml"
$OutputPath = Join-Path $ProjectRoot "network\inc\config\Const.h"

# Convert snake_case to PascalCase
function ConvertTo-PascalCase
{
    param([string]$text)
    $parts = $text -split '_'
    $result = ($parts | ForEach-Object {
        if ($_.Length -gt 0)
        {
            $_.Substring(0, 1).ToUpper() + $_.Substring(1).ToLower()
        }
    }) -join ''
    return $result
}

# Convert snake_case to camelCase
function ConvertTo-CamelCase
{
    param([string]$text)
    $pascal = ConvertTo-PascalCase $text
    if ($pascal.Length -gt 0)
    {
        return $pascal.Substring(0, 1).ToLower() + $pascal.Substring(1)
    }
    return $pascal
}

# Infer C++ type from TOML value
function Get-CppType
{
    param([string]$value)
    if ($value -match '^-?\d+$')
    {
        return "int"
    }
    if ($value -match '^-?\d+\.\d+$')
    {
        return "double"
    }
    if ($value -eq 'true' -or $value -eq 'false')
    {
        return "bool"
    }
    return "const char*"
}

# Format value for C++
function Format-CppValue
{
    param([string]$value, [string]$type)
    if ($type -eq "const char*")
    {
        return "`"$value`""
    }
    return $value
}

if (-not (Test-Path $TomlPath))
{
    Write-Error "TOML file not found: $TomlPath"
    exit 1
}

$lines = Get-Content -Path $TomlPath
$sections = @{ }
$currentSection = $null

foreach ($line in $lines)
{
    $line = $line.Trim()

    # Skip empty lines and comments
    if ($line -eq '' -or $line.StartsWith('#'))
    {
        continue
    }

    # Section header
    if ($line -match '^\[(\w+)\]$')
    {
        $currentSection = $Matches[1]
        $sections[$currentSection] = @()
        continue
    }

    # Key-value pair
    if ($line -match '^(\w+)\s*=\s*(.+)$' -and $currentSection)
    {
        $key = $Matches[1]
        $value = $Matches[2].Trim().Trim('"')
        $sections[$currentSection] += @{
            Key = $key
            Value = $value
            CppType = Get-CppType $value
        }
    }
}

# Generate C++ code
$relativePath = $TomlPath.Replace($ProjectRoot, "").TrimStart("\", "/").Replace("\", "/")

$code = @"
#pragma once
// Auto-generated from: $relativePath
// Do not edit manually. Run scripts/parse_compile_cfg.ps1 to regenerate.

namespace highp::net {

/// <summary>
/// Compile-time network constants.
/// TOML sections are represented as nested structs.
/// </summary>
struct Const {

"@

foreach ($sectionName in $sections.Keys | Sort-Object)
{
    $structName = ConvertTo-PascalCase $sectionName
    $entries = $sections[$sectionName]

    $code += @"
	/// <summary>[$sectionName] section</summary>
	struct $structName {

"@

    foreach ($entry in $entries)
    {
        $fieldName = ConvertTo-CamelCase $entry.Key
        $cppType = $entry.CppType
        $cppValue = Format-CppValue $entry.Value $cppType

        $code += @"
		static constexpr $cppType $fieldName = $cppValue;

"@
    }

    $code += @"
	};

"@
}

$code += @"
};

} // namespace highp::net
"@

$code | Out-File -FilePath $OutputPath -Encoding UTF8 -NoNewline
Write-Host "Generated: $OutputPath"
