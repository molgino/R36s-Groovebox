#!/bin/bash
# Schaltet den OTG-Port im Device Tree frei (eine Zeile: otg-port status = "okay"),
# damit der R36S per Kabel am PC als USB-Geraet erscheinen kann (PC-Modus).
# Sicherung: <name>.dtb.orig auf der BOOT-Partition. Rueckgaengig: "Groovebox - OTG-Port Original.sh"
# oder am PC auf der SD-Karte (Partition BOOT) die .orig-Datei zurueck umbenennen.
[ "$(id -u)" != 0 ] && exec sudo bash "$0" "$@"
GBDIR="/roms/ports/groovebox"; [ -d "/roms2/ports/groovebox" ] && GBDIR="/roms2/ports/groovebox"
SRC="$GBDIR/dtb-otg"
say() { echo "$*"; echo "$*" > /dev/tty1 2>/dev/null; }
BOOT=""
for d in /boot /flash /mnt/boot; do [ -f "$d/boot.ini" ] && BOOT=$d && break; done
[ -z "$BOOT" ] && { say "FEHLER: boot.ini nicht gefunden"; sleep 5; exit 1; }
mount -o remount,rw "$BOOT" 2>/dev/null
DONE=0
for name in $(grep -o '[A-Za-z0-9_.-]*\.dtb' "$BOOT/boot.ini" | sort -u); do
  [ -f "$BOOT/$name" ] || continue
  line=$(grep " $name\$" "$SRC/md5.txt") || { echo "$name: unbekannt, uebersprungen"; continue; }
  orig=$(echo "$line" | cut -d' ' -f1); patched=$(echo "$line" | cut -d' ' -f2)
  cur=$(md5sum "$BOOT/$name" | cut -d' ' -f1)
  if [ "$cur" = "$patched" ]; then say "$name: OTG-Port ist bereits frei"; continue; fi
  if [ "$cur" != "$orig" ]; then say "$name: unerwartete Version - zur Sicherheit nicht veraendert"; continue; fi
  [ -f "$BOOT/$name.orig" ] || cp "$BOOT/$name" "$BOOT/$name.orig"
  cp "$SRC/$name" "$BOOT/$name.new" && mv -f "$BOOT/$name.new" "$BOOT/$name"
  [ "$(md5sum "$BOOT/$name" | cut -d' ' -f1)" = "$patched" ] && { say "$name: OTG-Port freigeschaltet (Sicherung: $name.orig)"; DONE=1; } \
    || { cp "$BOOT/$name.orig" "$BOOT/$name"; say "$name: Kopierfehler - Original wiederhergestellt"; }
done
sync
if [ "$DONE" = 1 ]; then say "Neustart in 5 Sekunden ..."; sleep 5; reboot; else say "Nichts geaendert."; sleep 5; fi
