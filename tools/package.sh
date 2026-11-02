#!/usr/bin/env bash
# Baut das ArkOS-Paket dist/r36s-groovebox-arkos.zip aus arkos/ + groovebox-aarch64 + README.
set -euo pipefail
cd "$(dirname "$0")/.."
[ -f groovebox-aarch64 ] || ./tools/cross-build.sh
STAGE=$(mktemp -d)
cp -r arkos/. "$STAGE/"
cp groovebox-aarch64 "$STAGE/groovebox/groovebox"
chmod +x "$STAGE/groovebox/groovebox" "$STAGE"/*.sh "$STAGE/groovebox/pc-modus.sh"
mkdir -p "$STAGE/groovebox/projects"
cp README.md "$STAGE/groovebox/LIESMICH.md"
git rev-parse --short HEAD > "$STAGE/groovebox/VERSION" 2>/dev/null || echo dev > "$STAGE/groovebox/VERSION"
mkdir -p dist
rm -f dist/r36s-groovebox-arkos.zip
(cd "$STAGE" && zip -qr - .) > dist/r36s-groovebox-arkos.zip
rm -rf "$STAGE"
echo ">> dist/r36s-groovebox-arkos.zip"
