#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-desktop"
APP_NAME="crucible-fea-desktop"

echo "=== Crucible-FEA Desktop Builder ==="
echo ""

# Configure
echo "[1/3] Configuring..."
cmake -B "$BUILD_DIR" -DCRUCIBLE_FEA_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release

# Build
echo "[2/3] Building..."
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu)"

# Copy to /Applications
echo "[3/3] Installing to /Applications..."
rm -rf "/Applications/${APP_NAME}.app"
cp -R "$BUILD_DIR/${APP_NAME}.app" /Applications/

echo ""
echo "=== Done! ==="
echo "App installed at: /Applications/${APP_NAME}.app"
echo "Launch with: open /Applications/${APP_NAME}.app"
echo ""

# Launch the app
open "/Applications/${APP_NAME}.app"