#!/bin/bash
# Lokal bauen und testen (Linux / WSL-Debian), ohne Commit. Aufruf unter Windows ueber tools/dev.ps1.
#   tools/dev.sh os      -> App-Update fuer das Groovebox-OS (arm64, DRM): dist/groovebox-os-update.tgz
#   tools/dev.sh test    -> Unit-Tests (ASan/UBSan) + ThreadSanitizer + Selbsttest (ASan) wie die CI, aber mit Test-Samples
#   tools/dev.sh quick   -> nur Unit-Tests (schnell)
# Gebaut wird in einer Kopie unter ~/synthcity-build/<ziel> (schneller als auf /mnt/c, native und arm64 getrennt).
# Einmalig (Debian 13): dpkg --add-architecture arm64; apt install build-essential pkg-config python3-numpy
#   libsdl2-dev libasound2-dev libdrm-dev g++-aarch64-linux-gnu libsdl2-dev:arm64 libasound2-dev:arm64 libdrm-dev:arm64
set -e
SRC="$(cd "$(dirname "$0")/.." && pwd)"
MODE=${1:-os}
J=$(nproc)
sync_to() {   # Quellen spiegeln (Objektdateien und Binaries der Kopie bleiben fuer inkrementelle Builds)
    mkdir -p "$1"
    tar -C "$SRC" --exclude=./.git --exclude='./src/*.o' --exclude=./groovebox --exclude=./dist --exclude=./.sysroot \
        --exclude=./os/work --exclude=./os/out --exclude=./tests/unit --exclude=./tests/unit-tsan -cf - . | tar -C "$1" -xf -
    find "$1/tools" "$1/os/rootfs/overlay" -type f \( -name '*.sh' -o -path '*/sbin/*' \) -exec sed -i 's/\r$//' {} +   # Windows-Checkout: CRLF
    # tar setzt die Zeiten der Quellen: nur Geaendertes wird neu uebersetzt
}
case "$MODE" in
os)
    B=~/synthcity-build/arm64
    sync_to "$B"
    cd "$B"
    export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
    CXXFLAGS=-mcpu=cortex-a35 make -j"$J" DRM=1 CXX=aarch64-linux-gnu-g++ CC_LINK=aarch64-linux-gnu-gcc \
        SDL_CFLAGS="$(pkg-config --cflags sdl2)" SDL_LIBS="$(pkg-config --libs sdl2)"   # CXXFLAGS als Umgebung: das Makefile haengt an
    aarch64-linux-gnu-strip -o groovebox.stripped groovebox
    VER="$(git -C "$SRC" rev-parse --short HEAD 2>/dev/null || echo lokal)$(git -C "$SRC" diff --quiet HEAD 2>/dev/null || echo +lokal)"
    # os-update.sh nimmt die Version aus git; hier ohne .git -> eigene VERSION nachtragen
    ./tools/os-update.sh groovebox.stripped >/dev/null
    T=$(mktemp -d); tar -xzf dist/groovebox-os-update.tgz -C "$T"
    echo "$VER $(date +%Y-%m-%d\ %H:%M)" > "$T/VERSION"
    tar -czf dist/groovebox-os-update.tgz -C "$T" .; rm -rf "$T"
    mkdir -p "$SRC/dist"; cp dist/groovebox-os-update.tgz "$SRC/dist/"
    echo ">> dist/groovebox-os-update.tgz ($VER, $(du -h dist/groovebox-os-update.tgz | cut -f1))"
    ;;
test|quick)
    B=~/synthcity-build/native
    sync_to "$B"
    cd "$B"
    [ -d samples ] || python3 tools/gensamples.py samples >/dev/null   # sonst laufen Explorer-/Pool-Tests nicht
    [ -d samples/sub/tiefer ] || { mkdir -p samples/sub/tiefer; cp "$(ls samples/*/*.wav | head -1)" samples/sub/tiefer/test.wav; }
    rm -f tests/unit; make test SAN=1
    [ "$MODE" = quick ] && exit 0
    setarch "$(uname -m)" -R make test-tsan   # WSL: TSan braucht ein festes Speicher-Layout
    make clean >/dev/null; make -j"$J" SAN=1
    rm -rf projects presets settings.cfg
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0 ./groovebox --selftest ${2:-30}
    ;;
*) echo "tools/dev.sh os|test|quick"; exit 1 ;;
esac
