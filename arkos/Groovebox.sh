#!/bin/bash
# Synth City - Starter fuer ArkOS (Ports-Menue)
GBDIR="/roms/ports/groovebox"
[ -d "/roms2/ports/groovebox" ] && GBDIR="/roms2/ports/groovebox"
cd "$GBDIR" || exit 1
chmod +x ./groovebox 2>/dev/null
export SDL_AUDIODRIVER=alsa
./groovebox "$@" > "$GBDIR/groovebox.log" 2>&1
printf "\033c" > /dev/tty1 2>/dev/null
