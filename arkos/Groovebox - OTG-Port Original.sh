#!/bin/bash
# Stellt die originale Device-Tree-Datei wieder her (macht "OTG-Port freischalten" rueckgaengig).
[ "$(id -u)" != 0 ] && exec sudo bash "$0" "$@"
say() { echo "$*"; echo "$*" > /dev/tty1 2>/dev/null; }
BOOT=""
for d in /boot /flash /mnt/boot; do [ -f "$d/boot.ini" ] && BOOT=$d && break; done
[ -z "$BOOT" ] && { say "FEHLER: boot.ini nicht gefunden"; sleep 5; exit 1; }
mount -o remount,rw "$BOOT" 2>/dev/null
N=0
for o in "$BOOT"/*.dtb.orig; do
  [ -f "$o" ] || continue
  cp "$o" "${o%.orig}" && rm -f "$o" && N=$((N+1)) && say "wiederhergestellt: $(basename "${o%.orig}")"
done
sync
if [ "$N" -gt 0 ]; then say "Neustart in 5 Sekunden ..."; sleep 5; reboot; else say "Keine Sicherung gefunden - nichts zu tun."; sleep 5; fi
