#!/bin/bash
# PC-Modus fuer den R36S: schaltet den OTG-Port vom Host- in den Geraetemodus.
# Der R36S erscheint dann am PC (Ableton, MIDI-OX, ...) als USB-MIDI-Geraet "R36S Groovebox",
# mit "audio" zusaetzlich als USB-Audio-Interface (2 Kanaele, 48 kHz).
#   pc-modus.sh an [audio] [nosleep]   |   pc-modus.sh aus [nosleep]
[ "$(id -u)" != 0 ] && exec sudo bash "$0" "$@"
GBDIR="$(cd "$(dirname "$0")" && pwd)"
LOG="$GBDIR/pc-modus.log"
G=/sys/kernel/config/usb_gadget/groovebox
ARGS=" $* "
NOSLEEP=""; [[ "$ARGS" == *" nosleep "* ]] && NOSLEEP=1
AUDIO="";   [[ "$ARGS" == *" audio "* ]] && AUDIO=1
pause() { [ -z "$NOSLEEP" ] && sleep "$1"; }
say() { echo "$*"; [ -z "$NOSLEEP" ] && echo "$*" > /dev/tty1 2>/dev/null; }

teardown() {
  [ -d "$G" ] || return 0
  echo "" > "$G/UDC" 2>/dev/null
  rm -f "$G"/configs/c.1/*.usb0 2>/dev/null
  rmdir "$G"/configs/c.1/strings/0x409 "$G"/configs/c.1 "$G"/functions/* "$G"/strings/0x409 "$G" 2>/dev/null
}
setrole() {   # $1 = host | peripheral
  for f in $(find /sys/devices/platform -name otg_mode 2>/dev/null); do echo "$1" > "$f" 2>/dev/null && echo "otg_mode=$1: $f"; done
  local r=host; [ "$1" = peripheral ] && r=device
  for f in /sys/class/usb_role/*/role; do [ -e "$f" ] && echo "$r" > "$f" 2>/dev/null && echo "usb_role=$r: $f"; done
}

{
echo "=== $(date) pc-modus $*"
case "$1" in
aus)
  teardown
  setrole host
  say "PC-Modus aus - OTG-Port ist wieder Host (Keyboards, DACs, S-1 ...)"
  ;;
*)
  modprobe libcomposite 2>/dev/null; modprobe usb_f_midi 2>/dev/null
  [ -n "$AUDIO" ] && modprobe usb_f_uac2 2>/dev/null
  mountpoint -q /sys/kernel/config || mount -t configfs none /sys/kernel/config 2>/dev/null
  if [ ! -d /sys/kernel/config/usb_gadget ]; then
    say "FEHLER: Kernel ohne USB-Gadget (configfs)"; pause 5; exit 1
  fi
  teardown
  setrole peripheral
  sleep 1
  UDC=$(ls /sys/class/udc 2>/dev/null | head -1)
  if [ -z "$UDC" ]; then
    setrole host
    say "FEHLER: kein UDC - Kernel erlaubt nur Host-Modus"; pause 5; exit 1
  fi
  mkdir -p "$G" && cd "$G" || exit 1
  echo 0x1d6b > idVendor; echo 0x0104 > idProduct; echo 0x0100 > bcdDevice; echo 0x0200 > bcdUSB
  mkdir -p strings/0x409
  echo "R36S-0001" > strings/0x409/serialnumber
  echo "R36S" > strings/0x409/manufacturer
  echo "R36S Groovebox" > strings/0x409/product
  mkdir -p configs/c.1/strings/0x409
  echo "Groovebox" > configs/c.1/strings/0x409/configuration
  echo 100 > configs/c.1/MaxPower
  if ! mkdir -p functions/midi.usb0 2>/dev/null; then
    teardown; setrole host
    say "FEHLER: Kernel ohne USB-MIDI-Gadget (usb_f_midi)"; pause 5; exit 1
  fi
  echo "R36S Groovebox" > functions/midi.usb0/id 2>/dev/null
  ln -s functions/midi.usb0 configs/c.1/
  if [ -n "$AUDIO" ]; then
    if mkdir -p functions/uac2.usb0 2>/dev/null; then
      echo 3 > functions/uac2.usb0/p_chmask; echo 48000 > functions/uac2.usb0/p_srate; echo 2 > functions/uac2.usb0/p_ssize
      echo 3 > functions/uac2.usb0/c_chmask; echo 48000 > functions/uac2.usb0/c_srate; echo 2 > functions/uac2.usb0/c_ssize
      ln -s functions/uac2.usb0 configs/c.1/
    else
      echo "Hinweis: kein USB-Audio-Gadget (usb_f_uac2) im Kernel - nur MIDI"
    fi
  fi
  if echo "$UDC" > UDC; then
    say "PC-Modus aktiv ($UDC): R36S mit dem PC verbinden (am besten USB-A-auf-USB-C-Kabel)"
  else
    teardown; setrole host
    say "FEHLER: Gadget-Aktivierung fehlgeschlagen"; pause 5; exit 1
  fi
  ;;
esac
echo "--- UDC: $(ls /sys/class/udc 2>/dev/null)  Zustand: $(cat /sys/class/udc/*/state 2>/dev/null)"
} >> "$LOG" 2>&1
RC=$?
pause 3
exit $RC
