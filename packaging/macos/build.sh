#!/bin/bash
set -e

# Run from the repository root so all paths below stay project-relative.
cd "$(dirname "$0")/../.."

# ---- setup ----
APP_NAME="Tokri"
BUILD_DIR="build/Release"
APP="${BUILD_DIR}/${APP_NAME}.app"
OUT_DIR="dist"

DMG_ROOT="$OUT_DIR/dmgroot"
DMG_PATH="$OUT_DIR/${APP_NAME}.dmg"

echo "▶ setup"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# use the macdeployqt that matches the Qt the app was built with
QT_PREFIX=$(sed -n 's/^CMAKE_PREFIX_PATH:PATH=//p' "${BUILD_DIR}/CMakeCache.txt" | cut -d';' -f1)
MACDEPLOYQT="${QT_PREFIX}/bin/macdeployqt"
if [ ! -x "$MACDEPLOYQT" ]; then
  echo "⚠ ${MACDEPLOYQT} not found, falling back to PATH macdeployqt"
  MACDEPLOYQT="macdeployqt"
fi

# ---- deploy ----
echo "▶ macdeployqt (${MACDEPLOYQT})"
rm -rf "$APP/Contents/Frameworks" "$APP/Contents/PlugIns"
"$MACDEPLOYQT" "$APP" -always-overwrite

# ---- dmg ----
echo "▶ dmg"
mkdir -p "$DMG_ROOT"
cp -R "$APP" "$DMG_ROOT/"
ln -s /Applications "$DMG_ROOT/Applications"

hdiutil create \
  -volname "$APP_NAME" \
  -srcfolder "$DMG_ROOT" \
  -ov -format UDZO \
  "$DMG_PATH"

# ---- teardown ----
rm -rf "$DMG_ROOT"

echo "▶ done"
ls -lh "$DMG_PATH"
