#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-desktop"

echo "=== Cauchy FEA Desktop Builder ==="
echo ""

# Configure
echo "[1/3] Configuring..."
cmake -B "$BUILD_DIR" -DCAUCHY_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release

# Build
echo "[2/3] Building..."
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu)"

# Copy to /Applications
echo "[3/3] Installing to /Applications..."
cp -R "$BUILD_DIR/cauchy-desktop.app" /Applications/

echo ""
echo "=== Done! ==="
echo "App installed at: /Applications/Cauchy.app"
echo "Launch with: open /Applications/Cauchy.app"
echo ""

# Launch the app
open /Applications/Cauchy.app