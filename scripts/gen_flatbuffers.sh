#!/usr/bin/env bash
# FlatBuffers schema compilation script

# Configuration
FLATC_PATH="flatc"
SCHEMAS_DIR="./serialize/flatbuf/schemas"
OUTPUT_DIR="./generated"

# Colors for output
ERROR_COLOR="\033[0;31m"    # Red
SUCCESS_COLOR="\033[0;32m"  # Green
INFO_COLOR="\033[0;36m"     # Cyan
YELLOW_COLOR="\033[0;33m"   # Yellow
RESET_COLOR="\033[0m"       # Reset

# Check if flatc is available
if ! command -v "$FLATC_PATH" &> /dev/null; then
    echo -e "${ERROR_COLOR}Error: flatc compiler not found in PATH${RESET_COLOR}"
    echo -e "${ERROR_COLOR}Please install FlatBuffers or add flatc to your PATH${RESET_COLOR}"
    exit 1
fi

# Check if schemas directory exists
if [ ! -d "$SCHEMAS_DIR" ]; then
    echo -e "${ERROR_COLOR}Error: Schemas directory not found: $SCHEMAS_DIR${RESET_COLOR}"
    exit 1
fi

# Create output directory if it doesn't exist
if [ ! -d "$OUTPUT_DIR" ]; then
    echo -e "${INFO_COLOR}Creating output directory: $OUTPUT_DIR${RESET_COLOR}"
    mkdir -p "$OUTPUT_DIR"
fi

# Find all .fbs files recursively
mapfile -t FBS_FILES < <(find "$SCHEMAS_DIR" -type f -name "*.fbs")

# Check if any files were found
if [ ${#FBS_FILES[@]} -eq 0 ]; then
    echo -e "${YELLOW_COLOR}Warning: No .fbs files found in $SCHEMAS_DIR${RESET_COLOR}"
    exit 0
fi

echo -e "${INFO_COLOR}Found ${#FBS_FILES[@]} FlatBuffers schema file(s)${RESET_COLOR}"
echo ""

# Compile each schema
SUCCESS_COUNT=0
FAIL_COUNT=0

for file in "${FBS_FILES[@]}"; do
    # Get relative path for display
    RELATIVE_PATH="${file#./}"
    echo -e "${INFO_COLOR}Compiling: $RELATIVE_PATH${RESET_COLOR}"

    # Compile with C++ output
    # -I specifies include path for imports
    # -o specifies output directory
    # --cpp generates C++ code
    # --scoped-enums generates scoped enums (C++11)
    if OUTPUT=$("$FLATC_PATH" --cpp --scoped-enums -I "$SCHEMAS_DIR" -o "$OUTPUT_DIR" "$file" 2>&1); then
        echo -e "  ${SUCCESS_COLOR}[OK] Success${RESET_COLOR}"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        echo -e "  ${ERROR_COLOR}[FAIL] Failed${RESET_COLOR}"
        echo -e "  ${ERROR_COLOR}$OUTPUT${RESET_COLOR}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done

echo ""
if [ $FAIL_COUNT -eq 0 ]; then
    SUMMARY_COLOR="$SUCCESS_COLOR"
else
    SUMMARY_COLOR="$YELLOW_COLOR"
fi
echo -e "${SUMMARY_COLOR}Compilation complete: $SUCCESS_COUNT succeeded, $FAIL_COUNT failed${RESET_COLOR}"

if [ $FAIL_COUNT -gt 0 ]; then
    exit 1
fi
