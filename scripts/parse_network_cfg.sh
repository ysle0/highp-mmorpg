#!/usr/bin/env bash
# parse_network_cfg.sh
# Parses config.runtime.toml and generates C++ runtime config struct header.
# TOML sections (depth <= 1) are converted to nested structs with Defaults.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

TOML_PATH="$PROJECT_ROOT/exec/echo/echo-server/config.runtime.toml"
OUTPUT_PATH="$PROJECT_ROOT/network/NetworkCfg.h"

# Convert snake_case to PascalCase
to_pascal_case() {
    local text="$1"
    local result=""
    IFS='_' read -ra parts <<< "$text"
    for part in "${parts[@]}"; do
        if [ -n "$part" ]; then
            result+="${part^}"
        fi
    done
    echo "$result"
}

# Convert snake_case to camelCase
to_camel_case() {
    local pascal=$(to_pascal_case "$1")
    if [ -n "$pascal" ]; then
        echo "${pascal,}"
    else
        echo "$pascal"
    fi
}

# Infer C++ type from TOML value
get_cpp_type() {
    local value="$1"
    if [[ "$value" =~ ^-?[0-9]+$ ]]; then
        echo "INT"
    elif [[ "$value" =~ ^-?[0-9]+\.[0-9]+$ ]]; then
        echo "double"
    elif [[ "$value" == "true" || "$value" == "false" ]]; then
        echo "bool"
    else
        echo "std::string"
    fi
}

# Format value for C++
format_cpp_value() {
    local value="$1"
    local type="$2"
    if [ "$type" == "std::string" ]; then
        echo "\"$value\""
    else
        echo "$value"
    fi
}

# Check if TOML file exists
if [ ! -f "$TOML_PATH" ]; then
    echo "Error: TOML file not found: $TOML_PATH" >&2
    exit 1
fi

# Parse TOML file
declare -A sections
declare -a section_order
current_section=""

while IFS= read -r line; do
    # Trim whitespace
    line=$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')

    # Skip empty lines and comments
    if [ -z "$line" ] || [[ "$line" =~ ^# ]]; then
        continue
    fi

    # Section header
    if [[ "$line" =~ ^\[([a-zA-Z0-9_]+)\]$ ]]; then
        current_section="${BASH_REMATCH[1]}"
        section_order+=("$current_section")
        sections["$current_section"]=""
        continue
    fi

    # Key-value pair
    if [[ "$line" =~ ^([a-zA-Z0-9_]+)[[:space:]]*=[[:space:]]*(.+)$ ]] && [ -n "$current_section" ]; then
        key="${BASH_REMATCH[1]}"
        value="${BASH_REMATCH[2]}"
        # Remove quotes and trim
        value=$(echo "$value" | sed 's/^[[:space:]]*"//;s/"[[:space:]]*$//')

        cpp_type=$(get_cpp_type "$value")
        camel_case=$(to_camel_case "$key")

        # Append to section data (format: key|value|type|camelCase)
        if [ -n "${sections[$current_section]}" ]; then
            sections["$current_section"]+=$'\n'
        fi
        sections["$current_section"]+="$key|$value|$cpp_type|$camel_case"
    fi
done < "$TOML_PATH"

# Generate relative path for header comment
RELATIVE_PATH="${TOML_PATH#$PROJECT_ROOT/}"
RELATIVE_PATH="${RELATIVE_PATH//\\//}"

# Generate C++ code directly to file
{
    echo "#pragma once"
    echo "// Auto-generated from: $RELATIVE_PATH"
    echo "// Do not edit manually. Run scripts/parse_network_cfg.sh to regenerate."
    echo ""
    echo "#include <filesystem>"
    echo "#include <runTime/Config.hpp>"
    echo "#include <stdexcept>"
    echo "#include <Windows.h>"
    echo ""
    echo "namespace highp::network {"
    echo ""
    echo "/// <summary>"
    echo "/// Runtime network configuration."
    echo "/// TOML sections are represented as nested structs."
    echo "/// </summary>"
    echo "struct NetworkCfg {"
    echo ""

    # Generate nested structs for each section
    for section_name in "${section_order[@]}"; do
        struct_name=$(to_pascal_case "$section_name")

        echo "	/// <summary>[$section_name] section</summary>"
        echo "	struct $struct_name {"
        echo ""

        # Member variables
        if [ -n "${sections[$section_name]}" ]; then
            while IFS='|' read -r key value cpp_type camel_case; do
                echo "		$cpp_type $camel_case;"
                echo ""
            done <<< "${sections[$section_name]}"
        fi

        # Defaults nested struct
        echo "		struct Defaults {"
        echo ""

        if [ -n "${sections[$section_name]}" ]; then
            while IFS='|' read -r key value cpp_type camel_case; do
                cpp_value=$(format_cpp_value "$value" "$cpp_type")
                echo "			static constexpr $cpp_type $camel_case = $cpp_value;"
                echo ""
            done <<< "${sections[$section_name]}"
        fi

        echo "		};"
        echo "	} $section_name;"
        echo ""
    done

    # Generate FromFile method
    echo "	/// <summary>Load from TOML file</summary>"
    echo "	static NetworkCfg FromFile(const std::filesystem::path& path) {"
    echo "		auto cfg = highp::config::Config::FromFile(path);"
    echo "		if (!cfg.has_value()) {"
    echo "			throw std::runtime_error(\"Failed to load config file: \" + path.string());"
    echo "		}"
    echo "		return FromCfg(cfg.value());"
    echo "	}"
    echo ""
    echo "	/// <summary>Load from Config object</summary>"
    echo "	static NetworkCfg FromCfg(const highp::config::Config& cfg) {"
    echo "		return NetworkCfg{"
    echo ""

    # Generate FromCfg initializers
    for section_name in "${section_order[@]}"; do
        struct_name=$(to_pascal_case "$section_name")

        echo "			.$section_name = {"
        echo ""

        if [ -n "${sections[$section_name]}" ]; then
            while IFS='|' read -r key value cpp_type camel_case; do
                toml_key="$section_name.$key"
                env_var=$(echo "${section_name}_${key}" | tr '[:lower:]' '[:upper:]')
                echo "				.$camel_case = cfg.Int(\"$toml_key\", $struct_name::Defaults::$camel_case, \"$env_var\"),"
                echo ""
            done <<< "${sections[$section_name]}"
        fi

        echo "			},"
        echo ""
    done

    echo "		};"
    echo "	}"
    echo ""
    echo "	/// <summary>Create with default values</summary>"
    echo "	static NetworkCfg WithDefaults() {"
    echo "		return NetworkCfg{"
    echo ""

    # Generate WithDefaults initializers
    for section_name in "${section_order[@]}"; do
        struct_name=$(to_pascal_case "$section_name")

        echo "			.$section_name = {"
        echo ""

        if [ -n "${sections[$section_name]}" ]; then
            while IFS='|' read -r key value cpp_type camel_case; do
                echo "				.$camel_case = $struct_name::Defaults::$camel_case,"
                echo ""
            done <<< "${sections[$section_name]}"
        fi

        echo "			},"
        echo ""
    done

    echo "		};"
    echo "	}"
    echo "};"
    echo ""
    echo "} // namespace highp::network"
} > "$OUTPUT_PATH"

echo "Generated: $OUTPUT_PATH"
