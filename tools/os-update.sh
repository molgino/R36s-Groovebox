#!/bin/bash
# Baut das App-Update fuer das Groovebox-OS: dist/groovebox-os-update.tgz
# (Programm mit DRM-Ausgabe + Skripte). Aufspielen: Upload-Webseite, Laufwerk GROOVEBOX/update/ oder deploy-Skript.
#   tools/os-update.sh [groovebox-binary]      (Binary vorher mit "make DRM=1" auf/fuer arm64 bauen)
set -e
cd "$(dirname "$0")/.."
BIN=${1:-groovebox}
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
install -m 755 "$BIN" "$T/groovebox"
strip "$T/groovebox" 2>/dev/null || true
cp os/rootfs/overlay/opt/groovebox/pc-modus.sh "$T/"
mkdir -p "$T/sbin"
cp os/rootfs/overlay/usr/local/sbin/* "$T/sbin/"
echo "$(git rev-parse --short HEAD 2>/dev/null || echo lokal) $(date +%Y-%m-%d)" > "$T/VERSION"
mkdir -p dist
tar -czf dist/groovebox-os-update.tgz -C "$T" .
echo ">> dist/groovebox-os-update.tgz ($(cat "$T/VERSION"))"
