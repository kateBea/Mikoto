#!/bin/bash

# Need to specify -fshader-stage=<stage> if it cannot be deduced. See glslc --help for more info
# See issue: https://github.com/WebGLTools/GL-Shader-Validator/issues/9

# ---------------------------------------------------------------------------
# Vulkan GLSL -> SPIR-V shader compiler script
# Automatically detects shader stage from filename suffix or directory.
# Requires `glslc` to be in PATH.
# ---------------------------------------------------------------------------

set -e

# Optional: set shader directory to script's location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Supported stage mappings
declare -A STAGE_MAP=(
    ["vert"]="vertex"
    ["frag"]="fragment"
    ["comp"]="compute"
)

echo "Compiling shaders in: $SCRIPT_DIR"
echo "------------------------------------------------------------"

# Find all GLSL files (case-sensitive)
shopt -s nullglob
for file in *.glsl; do
    base="${file%.*}"
    output="${base}.sprv"

    # Determine stage
    stage=""
    for key in "${!STAGE_MAP[@]}"; do
        if [[ "$base" == *"${key}"* || "$base" == *"${key^}"* ]]; then
            stage="${STAGE_MAP[$key]}"
            break
        fi
    done

    if [[ -z "$stage" ]]; then
        echo "Skipping $file — unknown shader stage"
        continue
    fi

    echo "-> Compiling $file → $output ($stage)"
    glslc -O -fshader-stage="$stage" "$file" -o "$output"
done

echo "------------------------------------------------------------"
echo "Shader compilation complete."

