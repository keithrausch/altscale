#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PRESET="${1:-debug}"

echo "========================================"
echo " Building project"
echo "========================================"
echo "Preset: $PRESET"
echo

# Configure the project
"$SCRIPT_DIR/container.sh" \
    cmake --preset "$PRESET"

# Build the project
"$SCRIPT_DIR/container.sh" \
    cmake --build --preset "$PRESET" --parallel
