#!/usr/bin/env bash
# Cross-Build der Groovebox fuer den R36S (ArkOS, aarch64) auf einem x86-Linux (Ubuntu/Debian).
# Baut gegen einen Ubuntu-18.04-arm64-Sysroot (glibc 2.27, SDL 2.0.8), damit das Programm
# auf ArkOS 2.0 (Ubuntu 19.10, glibc 2.30) laeuft und dort die geraeteeigene SDL2 benutzt.
# Voraussetzung:  sudo apt install g++-aarch64-linux-gnu curl
set -euo pipefail
cd "$(dirname "$0")/.."
SYSROOT=${SYSROOT:-$PWD/.sysroot}
MIRROR=http://ports.ubuntu.com/ubuntu-ports
PKGS=(
  pool/main/g/glibc/libc6_2.27-3ubuntu1.6_arm64.deb
  pool/main/g/glibc/libc6-dev_2.27-3ubuntu1.6_arm64.deb
  pool/main/l/linux/linux-libc-dev_4.15.0-213.224_arm64.deb
  pool/universe/libs/libsdl2/libsdl2-dev_2.0.8+dfsg1-1ubuntu1.18.04.4_arm64.deb
  pool/universe/libs/libsdl2/libsdl2-2.0-0_2.0.8+dfsg1-1ubuntu1.18.04.4_arm64.deb
  pool/main/a/alsa-lib/libasound2_1.1.3-5ubuntu0.6_arm64.deb
  pool/main/a/alsa-lib/libasound2-dev_1.1.3-5ubuntu0.6_arm64.deb
)
if [ ! -f "$SYSROOT/.done2" ]; then
  echo ">> Sysroot anlegen in $SYSROOT"
  mkdir -p "$SYSROOT/debs"
  for p in "${PKGS[@]}"; do
    f="$SYSROOT/debs/$(basename "$p")"
    [ -f "$f" ] || curl -fsSL -o "$f" "$MIRROR/$p"
    dpkg-deb -x "$f" "$SYSROOT"
  done
  # absolute Symlinks im Sysroot relativ machen
  ( cd "$SYSROOT/usr/lib/aarch64-linux-gnu"
    for l in $(find . -maxdepth 1 -type l); do
      t=$(readlink "$l"); case "$t" in /*) ln -sf "$SYSROOT$t" "$l";; esac
    done )
  touch "$SYSROOT/.done2"
fi
CROSS=${CROSS:-aarch64-linux-gnu-}
make clean >/dev/null
make OUT=groovebox-aarch64 \
  CXX="${CROSS}g++ --sysroot=$SYSROOT -mcpu=cortex-a35 -nostdinc -isystem $(${CROSS}gcc -print-file-name=include) -isystem $SYSROOT/usr/include/aarch64-linux-gnu -isystem $SYSROOT/usr/include" \
  CC_LINK="${CROSS}gcc --sysroot=$SYSROOT -B$SYSROOT/usr/lib/aarch64-linux-gnu -L$SYSROOT/lib/aarch64-linux-gnu -L$SYSROOT/usr/lib/aarch64-linux-gnu -Wl,-rpath-link,$SYSROOT/lib/aarch64-linux-gnu -Wl,--allow-shlib-undefined" \
  SDL_CFLAGS="-I$SYSROOT/usr/include/SDL2 -D_REENTRANT" \
  SDL_LIBS="-lSDL2"
${CROSS}strip groovebox-aarch64
rm -f src/*.o
echo ">> fertig: groovebox-aarch64"
${CROSS}objdump -T groovebox-aarch64 | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1 | sed 's/^/   hoechste benoetigte glibc: /'
