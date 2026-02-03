#!/usr/bin/env bash
# parse_compile_cfg.sh
# Parses config.compile.toml and generates C++ constexpr struct header.
# TOML sections (depth <= 1) are converted to nested structs.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

TOML_PATH="$PROJECT_ROOT/network/config.compile.toml"
OUTPUT_PATH="$PROJECT_ROOT/network/Const.h"

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
        echo "const char*"
    fi
}

# Format value for C++
format_cpp_value() {
    local value="$1"
    local type="$2"
    if [ "$type" == "const char*" ]; then
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
        # Remove quotes
        value=$(echo "$value" | sed 's/^"//;s/"$//')

        cpp_type=$(get_cpp_type "$value")

        # Append to section data (format: key|value|type)
        if [ -n "${sections[$current_section]}" ]; then
            sections["$current_section"]+=$'\n'
        fi
        sections["$current_section"]+="$key|$value|$cpp_type"
    fi
done < "$TOML_PATH"

# Generate relative path for header comment
RELATIVE_PATH="${TOML_PATH#$PROJECT_ROOT/}"
RELATIVE_PATH="${RELATIVE_PATH//\\//}"

# Generate C++ code directly to file
{
    echo "#pragma once"
    echo "// Auto-generated from: $RELATIVE_PATH"
    echo "// Do not edit manually. Run scripts/parse_compile_cfg.sh to regenerate."
    echo ""
    echo "#include <Windows.h>"
    echo ""
    echo "namespace highp::network {"
    echo ""
    echo "/// <summary>"
    echo "/// Compile-time network constants."
    echo "/// TOML sections are represented as nested structs."
    echo "/// </summary>"
    echo "struct Const {"
    echo ""

    # Generate structs for each section (in order)
    for section_name in "${section_order[@]}"; do
        struct_name=$(to_pascal_case "$section_name")

        echo "	/// <summary>[$section_name] section</summary>"
        echo "	struct $struct_name {"
        echo ""

        # Process entries for this section
        if [ -n "${sections[$section_name]}" ]; then
            while IFS='|' read -r key value cpp_type; do
                field_name=$(to_camel_case "$key")
                cpp_value=$(format_cpp_value "$value" "$cpp_type")
                echo "		static constexpr $cpp_type $field_name = $cpp_value;"
                echo ""
            done <<< "${sections[$section_name]}"
        fi

        echo "	};"
        echo ""
    done

    echo "};"
    echo ""
    echo "} // namespace highp::network"
} > "$OUTPUT_PATH"

echo "Generated: $OUTPUT_PATH"
