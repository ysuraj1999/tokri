#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$(readlink -f "$0")")"

APP_ID="com.ysuraj.Tokri"
BUILD_DIR="build-flatpak"
REPO_DIR="repo"
BUNDLE="../dist/${APP_ID}.flatpak"
BRANCH="stable"

case "${1:-}" in
  "" | --local | local) MANIFEST="com.ysuraj.Tokri.yml" ;;
  *) MANIFEST="$1" ;;
esac

rm -rf "${BUILD_DIR}" "${REPO_DIR}" "${BUNDLE}"
mkdir -p "$(dirname "${BUNDLE}")"

flatpak-builder \
  --force-clean \
  --default-branch="${BRANCH}" \
  --repo="${REPO_DIR}" \
  "${BUILD_DIR}" \
  "${MANIFEST}"

flatpak build-update-repo "${REPO_DIR}"

flatpak build-bundle \
  "${REPO_DIR}" \
  "${BUNDLE}" \
  "${APP_ID}" \
  "${BRANCH}"

echo "Built ${BUNDLE}"
