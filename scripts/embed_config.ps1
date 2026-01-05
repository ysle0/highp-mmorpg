# embed_config.ps1
# Converts TOML file to C++ raw string literal header
# Usage: .\embed_config.ps1 -TomlPath "config.toml" -OutputPath "ConfigEmbedded.h" -VarName "CONFIG_TOML" -Namespace "myapp::config"

param(
    [Parameter(Mandatory=$true)]
    [string]$TomlPath,

    [Parameter(Mandatory=$true)]
    [string]$OutputPath,

    [string]$VarName = "EMBEDDED_CONFIG",

    [string]$Namespace = "highp::lib::config::embedded"
)

if (-not (Test-Path $TomlPath)) {
    Write-Error "TOML file not found: $TomlPath"
    exit 1
}

$content = Get-Content -Path $TomlPath -Raw

# Escape for C++ raw string
$header = @"
#pragma once
// Auto-generated from: $TomlPath
// Do not edit manually. Run embed_config.ps1 to regenerate.

namespace $Namespace
{

inline constexpr const char* $VarName = R"TOML(
$content)TOML";

} // namespace $Namespace
"@

$header | Out-File -FilePath $OutputPath -Encoding UTF8 -NoNewline
Write-Host "Generated: $OutputPath"
