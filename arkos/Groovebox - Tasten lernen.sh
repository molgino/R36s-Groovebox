#!/bin/bash
# Startet die Groovebox im Lernmodus: jede Taste einmal druecken, Belegung landet in input.cfg
GBDIR="/roms/ports/groovebox"
[ -d "/roms2/ports/groovebox" ] && GBDIR="/roms2/ports/groovebox"
exec "$(dirname "$0")/Groovebox.sh" --learn
