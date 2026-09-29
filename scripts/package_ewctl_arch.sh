#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REF_NAME="${GITHUB_REF_NAME:-}"
if [[ "$REF_NAME" =~ ^ewp-([0-9]+\.[0-9]+\.[0-9]+)$ ]]; then
  PKGVER="${BASH_REMATCH[1]}"
else
  PKGVER="$(sed -n 's/^pkgver=//p' "$ROOT/packaging/arch/ewctl/PKGBUILD" | head -n 1)"
fi
if [[ ! "$PKGVER" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Expected an ewp-X.Y.Z ref or valid pkgver in PKGBUILD; got '${REF_NAME:-unset}' / '$PKGVER'" >&2
  exit 2
fi

mkdir -p "$ROOT/.pio/release"
docker run --rm \
  -e PKGVER="$PKGVER" \
  -v "$ROOT:/workspace" \
  archlinux:base-devel \
  bash -euc '
    pacman -Syu --noconfirm --needed git pacman-contrib python python-pyserial python-rich esptool python-construct python-pygdbmi
    useradd --create-home builder
    mkdir -p /tmp/ewctl-pkgbuild /workspace/.pio/release
    cp /workspace/packaging/arch/ewctl/PKGBUILD /tmp/ewctl-pkgbuild/PKGBUILD
    sed -i "s/^pkgver=.*/pkgver=${PKGVER}/" /tmp/ewctl-pkgbuild/PKGBUILD
    chown -R builder:builder /tmp/ewctl-pkgbuild
    runuser -u builder -- bash -euc "cd /tmp/ewctl-pkgbuild && makepkg --cleanbuild --noconfirm"
    cp /tmp/ewctl-pkgbuild/*.pkg.tar.zst /workspace/.pio/release/
    repo-add /workspace/.pio/release/ersa-ewctl.db.tar.gz /workspace/.pio/release/ewctl-*.pkg.tar.zst
    # repo-add creates .db/.files aliases to the .tar.gz archives. Copy via
    # distinct temporary files before replacing those aliases with regular
    # files; direct cp follows the aliases and reports source == destination.
    cp -L /workspace/.pio/release/ersa-ewctl.db.tar.gz /tmp/ersa-ewctl.db
    mv -f /tmp/ersa-ewctl.db /workspace/.pio/release/ersa-ewctl.db
    cp -L /workspace/.pio/release/ersa-ewctl.files.tar.gz /tmp/ersa-ewctl.files
    mv -f /tmp/ersa-ewctl.files /workspace/.pio/release/ersa-ewctl.files
  '
