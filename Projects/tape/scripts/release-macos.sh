#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_FILE="$ROOT_DIR/projects/tape-macOS.xcodeproj"
CONFIG_FILE="$ROOT_DIR/config.h"
BUILD_ROOT="$ROOT_DIR/build-release"
DERIVED_DATA="$BUILD_ROOT/derived-data"
DIST_ROOT="$ROOT_DIR/dist/macos"

DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
SIGN_IDENTITY="${SIGN_IDENTITY:--}"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"
SKIP_BUILD="${SKIP_BUILD:-0}"
RUN_AUVAL="${RUN_AUVAL:-0}"

if [[ ! -f "$PROJECT_FILE/project.pbxproj" ]]; then
  echo "Project file not found: $PROJECT_FILE"
  exit 1
fi

if [[ ! -f "$CONFIG_FILE" ]]; then
  echo "Config file not found: $CONFIG_FILE"
  exit 1
fi

if [[ ! -d "$DEVELOPER_DIR" ]]; then
  echo "DEVELOPER_DIR is not valid: $DEVELOPER_DIR"
  exit 1
fi

if ! command -v xcodebuild >/dev/null 2>&1; then
  echo "xcodebuild not found in PATH"
  exit 1
fi

BINARY_NAME="$(awk -F\" '/#define BUNDLE_NAME/{print $2; exit}' "$CONFIG_FILE")"
VERSION_STR="$(awk -F\" '/#define PLUG_VERSION_STR/{print $2; exit}' "$CONFIG_FILE")"
AU_SUBTYPE="$(awk '/#define PLUG_UNIQUE_ID/{print $3; exit}' "$CONFIG_FILE" | tr -d "'")"
AU_MFR="$(awk '/#define PLUG_MFR_ID/{print $3; exit}' "$CONFIG_FILE" | tr -d "'")"

if [[ -z "$BINARY_NAME" || -z "$VERSION_STR" ]]; then
  echo "Failed to parse BUNDLE_NAME or PLUG_VERSION_STR from $CONFIG_FILE"
  exit 1
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
STAGE_DIR="$DIST_ROOT/${BINARY_NAME}-macos-${VERSION_STR}-${STAMP}"
AU_STAGE="$STAGE_DIR/${BINARY_NAME}.component"
VST3_STAGE="$STAGE_DIR/${BINARY_NAME}.vst3"
AU_ZIP="$STAGE_DIR/${BINARY_NAME}.component.zip"
VST3_ZIP="$STAGE_DIR/${BINARY_NAME}.vst3.zip"
FINAL_ZIP="$DIST_ROOT/${BINARY_NAME}-macos-${VERSION_STR}-${STAMP}.zip"

mkdir -p "$DIST_ROOT"

find_product() {
  local ext="$1"
  local derived_path="$DERIVED_DATA/Build/Products"
  local candidate=""

  if [[ -d "$derived_path" ]]; then
    candidate="$(find "$derived_path" -type d -name "${BINARY_NAME}.${ext}" | head -n 1 || true)"
  fi

  if [[ -n "$candidate" ]]; then
    echo "$candidate"
    return 0
  fi

  if [[ "$ext" == "component" ]]; then
    candidate="$HOME/Library/Audio/Plug-Ins/Components/${BINARY_NAME}.component"
  else
    candidate="$HOME/Library/Audio/Plug-Ins/VST3/${BINARY_NAME}.vst3"
  fi

  if [[ -d "$candidate" ]]; then
    echo "$candidate"
    return 0
  fi

  return 1
}

build_scheme() {
  local scheme="$1"
  echo "==> Building scheme: $scheme (Release)"
  DEVELOPER_DIR="$DEVELOPER_DIR" xcodebuild \
    -project "$PROJECT_FILE" \
    -scheme "$scheme" \
    -configuration Release \
    -derivedDataPath "$DERIVED_DATA" \
    ENABLE_USER_SCRIPT_SANDBOXING=NO \
    build
}

sign_bundle() {
  local bundle="$1"
  echo "==> Signing $bundle with identity: $SIGN_IDENTITY"

  if [[ "$SIGN_IDENTITY" == "-" ]]; then
    /usr/bin/codesign \
      --force \
      --deep \
      --strict \
      --sign "$SIGN_IDENTITY" \
      "$bundle"
    return 0
  fi

  /usr/bin/codesign \
    --force \
    --deep \
    --strict \
    --options runtime \
    --timestamp \
    --sign "$SIGN_IDENTITY" \
    "$bundle"
}

verify_bundle() {
  local bundle="$1"
  echo "==> Verifying signature: $bundle"
  /usr/bin/codesign --verify --deep --strict --verbose=2 "$bundle"
  /usr/bin/codesign -dv --verbose=4 "$bundle" >/dev/null 2>&1 || true
}

notarize_zip() {
  local zip_file="$1"

  if [[ -z "$NOTARY_PROFILE" ]]; then
    echo "==> NOTARY_PROFILE not set. Skipping notarization for $zip_file"
    return 0
  fi

  if [[ "$SIGN_IDENTITY" == "-" ]]; then
    echo "NOTARY_PROFILE is set, but SIGN_IDENTITY is ad-hoc (-). Use a Developer ID Application cert."
    exit 1
  fi

  echo "==> Notarizing $zip_file"
  DEVELOPER_DIR="$DEVELOPER_DIR" xcrun notarytool submit "$zip_file" \
    --keychain-profile "$NOTARY_PROFILE" \
    --wait
}

echo "==> Preparing macOS release for $BINARY_NAME v$VERSION_STR"
echo "    Root: $ROOT_DIR"
echo "    Project: $PROJECT_FILE"
echo "    Stage: $STAGE_DIR"
echo "    Developer dir: $DEVELOPER_DIR"

if [[ "$SKIP_BUILD" != "1" ]]; then
  build_scheme "macOS-AUv2"
  build_scheme "macOS-VST3"
else
  echo "==> SKIP_BUILD=1, skipping xcodebuild"
fi

AU_PRODUCT="$(find_product "component" || true)"
VST3_PRODUCT="$(find_product "vst3" || true)"

if [[ -z "$AU_PRODUCT" || ! -d "$AU_PRODUCT" ]]; then
  echo "Could not locate ${BINARY_NAME}.component in build products or ~/Library"
  exit 1
fi

if [[ -z "$VST3_PRODUCT" || ! -d "$VST3_PRODUCT" ]]; then
  echo "Could not locate ${BINARY_NAME}.vst3 in build products or ~/Library"
  exit 1
fi

echo "==> Found AU: $AU_PRODUCT"
echo "==> Found VST3: $VST3_PRODUCT"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
/usr/bin/ditto "$AU_PRODUCT" "$AU_STAGE"
/usr/bin/ditto "$VST3_PRODUCT" "$VST3_STAGE"

sign_bundle "$AU_STAGE"
sign_bundle "$VST3_STAGE"

verify_bundle "$AU_STAGE"
verify_bundle "$VST3_STAGE"

if [[ "$RUN_AUVAL" == "1" ]]; then
  echo "==> Running auval (AU subtype=$AU_SUBTYPE mfr=$AU_MFR)"
  auval -v aufx "$AU_SUBTYPE" "$AU_MFR" || true
fi

echo "==> Creating notarization zips"
/usr/bin/ditto -c -k --keepParent "$AU_STAGE" "$AU_ZIP"
/usr/bin/ditto -c -k --keepParent "$VST3_STAGE" "$VST3_ZIP"

notarize_zip "$AU_ZIP"
notarize_zip "$VST3_ZIP"

if [[ -n "$NOTARY_PROFILE" ]]; then
  echo "==> Stapling notarization tickets"
  DEVELOPER_DIR="$DEVELOPER_DIR" xcrun stapler staple "$AU_STAGE"
  DEVELOPER_DIR="$DEVELOPER_DIR" xcrun stapler staple "$VST3_STAGE"
fi

echo "==> Creating final distribution archive"
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$STAGE_DIR" "$FINAL_ZIP"

echo
echo "Release artifacts ready:"
echo "  Stage dir: $STAGE_DIR"
echo "  Final zip: $FINAL_ZIP"
echo "  AU zip:    $AU_ZIP"
echo "  VST3 zip:  $VST3_ZIP"
