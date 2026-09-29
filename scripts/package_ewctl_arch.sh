#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKGVER="${GITHUB_REF_NAME#ewp-}"
if [[ "$PKGVER" == "$GITHUB_REF_NAME" || ! "$PKGVER" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Expected GITHUB_REF_NAME like ewp-0.1.2; got '${GITHUB_REF_NAME:-unset}'" >&2
  exit 2
fi

mkdir -p "$ROOT/.pio/release"
docker run --rm \
  -e PKGVER="$PKGVER" \
  -v "$ROOT:/workspace" \
  archlinux:base-devel \
  bash -euc '
    pacman -Syu --noconfirm --needed git pacman-contrib python python-pyserial python-rich
    useradd --create-home builder
    mkdir -p /tmp/ewctl-pkgbuild /workspace/.pio/release
    cp /workspace/packaging/arch/ewctl/PKGBUILD /tmp/ewctl-pkgbuild/PKGBUILD
    sed -i "s/^pkgver=.*/pkgver=${PKGVER}/" /tmp/ewctl-pkgbuild/PKGBUILD
    chown -R builder:builder /tmp/ewctl-pkgbuild
    runuser -u builder -- bash -euc "cd /tmp/ewctl-pkgbuild && makepkg --cleanbuild --noconfirm"
    cp /tmp/ewctl-pkgbuild/*.pkg.tar.zst /workspace/.pio/release/
    repo-add /workspace/.pio/release/ewctl.db.tar.gz /workspace/.pio/release/ewctl-*.pkg.tar.zst
    cp -L /workspace/.pio/release/ewctl.db.tar.gz /workspace/.pio/release/ewctl.db
    cp -L /workspace/.pio/release/ewctl.files.tar.gz /workspace/.pio/release/ewctl.files
  '
