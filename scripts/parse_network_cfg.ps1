# parse_network_cfg.ps1
# Parses config.runtime.toml and generates C++ runtime config struct header.
# TOML sections (depth <= 1) are converted to nested structs with Defaults.

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

$TomlPath = Join-Path $ProjectRoot "exec\echo\echo-server\config.runtime.toml"
$OutputPath = Join-Path $ProjectRoot "network\inc\config\NetworkCfg.h"

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
    return "std::string"
}

# Format value for C++
function Format-CppValue
{
    param([string]$value, [string]$type)
    if ($type -eq "std::string")
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
$sections = [ordered]@{ }
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
            CamelCase = ConvertTo-CamelCase $key
        }
    }
}

# Generate C++ code
$relativePath = $TomlPath.Replace($ProjectRoot, "").TrimStart("\", "/").Replace("\", "/")

$code = @"
#pragma once
// Auto-generated from: $relativePath
// Do not edit manually. Run scripts/parse_network_cfg.ps1 to regenerate.

#include <filesystem>
#include <config/runTime/Config.hpp>
#include <stdexcept>
#include <string>

namespace highp::net {

/// <summary>
/// Runtime network configuration.
/// TOML sections are represented as nested structs.
/// </summary>
struct NetworkCfg {

"@

# Generate nested structs for each section
foreach ($sectionName in $sections.Keys)
{
    $structName = ConvertTo-PascalCase $sectionName
    $entries = $sections[$sectionName]

    $code += @"
	/// <summary>[$sectionName] section</summary>
	struct $structName {

"@

    # Member variables
    foreach ($entry in $entries)
    {
        $code += @"
        $( $entry.CppType ) $( $entry.CamelCase );

"@
    }

    # Defaults nested struct
    $code += @"

		struct Defaults {

"@
    foreach ($entry in $entries)
    {
        $cppValue = Format-CppValue $entry.Value $entry.CppType
        $code += @"
			static constexpr $( $entry.CppType ) $( $entry.CamelCase ) = $cppValue;

"@
    }
    $code += @"
		};
	} $sectionName;

"@
}

# Generate FromFile method
$code += @"

	/// <summary>Load from TOML file</summary>
	static NetworkCfg FromFile(const std::filesystem::path& path) {
		auto cfg = highp::config::Config::FromFile(path);
		if (!cfg.has_value()) {
			throw std::runtime_error("Failed to load config file: " + path.string());
		}
		return FromCfg(cfg.value());
	}

	/// <summary>Load from Config object</summary>
	static NetworkCfg FromCfg(const highp::config::Config& cfg) {
		return NetworkCfg{

"@

# Generate FromCfg initializers
foreach ($sectionName in $sections.Keys)
{
    $structName = ConvertTo-PascalCase $sectionName
    $entries = $sections[$sectionName]

    $code += @"
			.$sectionName = {

"@
    foreach ($entry in $entries)
    {
        $tomlKey = "$sectionName.$( $entry.Key )"
        $envVar = ($sectionName.ToUpper() + "_" + $entry.Key.ToUpper())
        $code += @"
				.$( $entry.CamelCase ) = cfg.Int("$tomlKey", $structName::Defaults::$( $entry.CamelCase ), "$envVar"),

"@
    }
    $code += @"
			},

"@
}

$code += @"
		};
	}

	/// <summary>Create with default values</summary>
	static NetworkCfg WithDefaults() {
		return NetworkCfg{

"@

# Generate WithDefaults initializers
foreach ($sectionName in $sections.Keys)
{
    $structName = ConvertTo-PascalCase $sectionName
    $entries = $sections[$sectionName]

    $code += @"
			.$sectionName = {

"@
    foreach ($entry in $entries)
    {
        $code += @"
				.$( $entry.CamelCase ) = $structName::Defaults::$( $entry.CamelCase ),

"@
    }
    $code += @"
			},

"@
}

$code += @"
		};
	}
};

} // namespace highp::net
"@

$code | Out-File -FilePath $OutputPath -Encoding UTF8 -NoNewline
Write-Host "Generated: $OutputPath"
