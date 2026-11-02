#!/bin/bash
# Sammelt Infos zu USB/Audio/MIDI in ports/groovebox/usb-diagnose.txt
# (vorher die USB-Geraete anstecken). Danach SD-Karte am PC auslesen.
GBDIR="/roms/ports/groovebox"
[ -d "/roms2/ports/groovebox" ] && GBDIR="/roms2/ports/groovebox"
OUT="$GBDIR/usb-diagnose.txt"
K=$(uname -r)
{
  echo "== Kernel"; uname -a
  echo; echo "== Device Tree"; tr -d '\0' < /proc/device-tree/model 2>/dev/null; echo
  for d in /boot /flash; do [ -f $d/boot.ini ] && { echo "boot.ini ($d):"; grep -n "dtb" $d/boot.ini; ls -la $d/*.dtb* 2>/dev/null; }; done
  echo; echo "== Gadget-Controller (/sys/class/udc) - leer = R36S kann NICHT als USB-Geraet am PC auftreten"
  ls /sys/class/udc
  echo; echo "== dr_mode im Device-Tree (host / peripheral / otg)"
  find /proc/device-tree -name dr_mode 2>/dev/null | while read -r f; do echo "$f: $(tr -d '\0' < "$f")"; done
  echo; echo "== Kernel-Konfiguration"
  (zcat /proc/config.gz 2>/dev/null || cat "/boot/config-$K" 2>/dev/null) | \
    grep -E "CONFIG_USB_GADGET=|CONFIG_USB_CONFIGFS|CONFIG_USB_CONFIGFS_F_MIDI|CONFIG_USB_CONFIGFS_F_UAC|CONFIG_USB_F_MIDI|CONFIG_SND_USB_AUDIO|CONFIG_SND_SEQUENCER|CONFIG_SND_RAWMIDI|CONFIG_USB_DWC2|CONFIG_USB_OTG" \
    || echo "(keine Kernel-Config gefunden)"
  echo; echo "== Kernel-Module (Datei vorhanden?)"
  find "/lib/modules/$K" \( -name "snd-usb-audio*" -o -name "snd-seq*" -o -name "usb_f_midi*" -o -name "usb_f_uac*" -o -name "libcomposite*" -o -name "dwc2*" \) 2>/dev/null
  echo; echo "== geladene Module"; lsmod
  echo; echo "== USB-Geraete"; lsusb 2>&1
  for d in /sys/bus/usb/devices/*; do [ -f "$d/product" ] && echo "$d: $(cat "$d/product" 2>/dev/null)"; done
  echo; echo "== ALSA"; cat /proc/asound/cards; ls -l /dev/snd; aplay -l 2>&1; amidi -l 2>&1; aconnect -l 2>&1
  echo; echo "== pc-modus.log"; cat "$GBDIR/pc-modus.log" 2>/dev/null || echo "(noch nicht benutzt)"
  echo; echo "== settings.cfg"; cat "$GBDIR/settings.cfg" 2>/dev/null
  echo; echo "== otg_mode / usb_role"; find /sys/devices/platform -name otg_mode 2>/dev/null | while read -r f; do echo "$f: $(cat "$f")"; done; for f in /sys/class/usb_role/*/role; do [ -e "$f" ] && echo "$f: $(cat "$f")"; done
  echo; echo "== configfs"; mount | grep configfs; ls /sys/kernel/config/usb_gadget 2>&1
  echo; echo "== dmesg (USB)"; dmesg 2>/dev/null | grep -iE "usb|dwc|otg|gadget|udc|snd" | tail -80
} > "$OUT" 2>&1
sync
echo "Diagnose gespeichert: $OUT" > /dev/tty1
sleep 3
