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
  [ -f /run/groovebox-dhcp.pid ] && kill "$(cat /run/groovebox-dhcp.pid)" 2>/dev/null; rm -f /run/groovebox-dhcp.pid "$GBDIR/netmidi.ip"
  [ -d "$G" ] || return 0
  rm -f "$G"/os_desc/c.1 2>/dev/null
  echo "" > "$G/UDC" 2>/dev/null
  rm -f "$G"/configs/c.1/*.usb0 2>/dev/null
  rmdir "$G"/functions/*/os_desc/* 2>/dev/null
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
  modprobe libcomposite 2>/dev/null
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
  # andere Gadgets (z. B. ADB) vom Controller loesen
  for og in /sys/kernel/config/usb_gadget/*; do
    [ "$og" = "$G" ] || [ ! -f "$og/UDC" ] && continue
    if [ -n "$(cat "$og/UDC" 2>/dev/null)" ]; then echo "loese anderes Gadget: $og"; echo "" > "$og/UDC" 2>/dev/null; fi
  done
  # verfuegbare Gadget-Funktionen ermitteln
  for m in usb_f_midi usb_f_uac2 usb_f_uac1 usb_f_acm usb_f_serial u_serial; do modprobe $m 2>/dev/null; done
  PR=/sys/kernel/config/usb_gadget/gbprobe
  mkdir -p "$PR"; AVAIL=""
  for fn in midi uac2 uac1 acm gser ecm rndis ncm ffs hid mass_storage; do
    if mkdir "$PR/functions/$fn.p" 2>/dev/null; then AVAIL="$AVAIL $fn"; rmdir "$PR/functions/$fn.p"; fi
  done
  rmdir "$PR" 2>/dev/null
  echo "Verfuegbare Gadget-Funktionen:${AVAIL:- keine}"

  mkdir -p "$G" && cd "$G" || exit 1
  echo 0x1d6b > idVendor; echo 0x0104 > idProduct; echo 0x0100 > bcdDevice; echo 0x0200 > bcdUSB
  # Composite mit IAD (wichtig fuer Windows bei Seriell + Audio)
  echo 0xEF > bDeviceClass; echo 0x02 > bDeviceSubClass; echo 0x01 > bDeviceProtocol
  mkdir -p strings/0x409
  echo "R36S-0001" > strings/0x409/serialnumber
  echo "R36S" > strings/0x409/manufacturer
  echo "R36S Groovebox" > strings/0x409/product
  mkdir -p configs/c.1/strings/0x409
  echo "Groovebox" > configs/c.1/strings/0x409/configuration
  echo 100 > configs/c.1/MaxPower
  MIDIOK=""; AUDOK=""
  if mkdir -p functions/midi.usb0 2>/dev/null; then
    echo "R36S Groovebox" > functions/midi.usb0/id 2>/dev/null
    ln -s functions/midi.usb0 configs/c.1/ && MIDIOK="USB-MIDI"
  elif mkdir -p functions/acm.usb0 2>/dev/null; then
    ln -s functions/acm.usb0 configs/c.1/ && MIDIOK="SERIELL"
    echo "kein usb_f_midi - MIDI laeuft ueber USB-Seriell (ttyGS0)"
  fi
  NETFN=""
  if [ -z "$MIDIOK" ]; then   # nur Netzwerk-Gadgets: MIDI ueber Netzwerk (RTP-MIDI)
    for fn in rndis ecm ncm; do
      if mkdir -p functions/$fn.usb0 2>/dev/null; then NETFN=$fn; break; fi
    done
    if [ "$NETFN" = rndis ]; then   # Windows bindet den RNDIS-Treiber dann automatisch
      echo 1 > os_desc/use 2>/dev/null; echo 0xcd > os_desc/b_vendor_code 2>/dev/null; echo MSFT100 > os_desc/qw_sign 2>/dev/null
      mkdir -p functions/rndis.usb0/os_desc/interface.rndis 2>/dev/null
      echo RNDIS > functions/rndis.usb0/os_desc/interface.rndis/compatible_id 2>/dev/null
      echo 5162001 > functions/rndis.usb0/os_desc/interface.rndis/sub_compatible_id 2>/dev/null
    fi
    if [ -n "$NETFN" ]; then
      ln -s functions/$NETFN.usb0 configs/c.1/ && MIDIOK="NETZWERK-MIDI"
      [ "$NETFN" = rndis ] && ln -s configs/c.1 os_desc/ 2>/dev/null
      echo "MIDI ueber USB-Netzwerk ($NETFN) + RTP-MIDI"
    else
      echo "weder MIDI-, Seriell- noch Netzwerk-Gadget vorhanden - kein MIDI zum PC"
    fi
  fi
  if [ -n "$AUDIO" ]; then
    if mkdir -p functions/uac2.usb0 2>/dev/null; then
      echo 3 > functions/uac2.usb0/p_chmask; echo 48000 > functions/uac2.usb0/p_srate; echo 2 > functions/uac2.usb0/p_ssize
      echo 3 > functions/uac2.usb0/c_chmask; echo 48000 > functions/uac2.usb0/c_srate; echo 2 > functions/uac2.usb0/c_ssize
      ln -s functions/uac2.usb0 configs/c.1/ && AUDOK="AUDIO"
    elif mkdir -p functions/uac1.usb0 2>/dev/null; then
      ln -s functions/uac1.usb0 configs/c.1/ && AUDOK="AUDIO(UAC1)"
    else
      echo "kein USB-Audio-Gadget (uac2/uac1) im Kernel"
    fi
  fi
  if [ -z "$MIDIOK" ] && [ -z "$AUDOK" ]; then
    cd /; teardown; setrole host
    say "FEHLER: Kernel hat keine passende Gadget-Funktion"; pause 5; exit 1
  fi
  if echo "$UDC" > UDC; then
    IPINFO=""
    if [ -n "$NETFN" ]; then
      sleep 1
      IF=$(cat functions/$NETFN.usb0/ifname 2>/dev/null); [ -z "$IF" ] || [ "$IF" = "(unnamed net_device)" ] && IF=usb0
      ip link set "$IF" up 2>/dev/null || ifconfig "$IF" up
      ip addr add 169.254.77.1/16 dev "$IF" 2>/dev/null || ifconfig "$IF" 169.254.77.1 netmask 255.255.0.0
      ip addr add 192.168.77.1/24 dev "$IF" 2>/dev/null || ifconfig "$IF:1" 192.168.77.1 netmask 255.255.255.0
      IP=169.254.77.1
      if command -v dnsmasq >/dev/null 2>&1; then
        dnsmasq --interface="$IF" --bind-interfaces --port=0 --dhcp-range=192.168.77.10,192.168.77.50,12h \
                --dhcp-option=3 --dhcp-option=6 --pid-file=/run/groovebox-dhcp.pid && IP=192.168.77.1
      elif command -v udhcpd >/dev/null 2>&1 || busybox udhcpd --help >/dev/null 2>&1; then
        printf "start 192.168.77.10\nend 192.168.77.50\ninterface %s\noption subnet 255.255.255.0\npidfile /run/groovebox-dhcp.pid\nlease_file /tmp/gb-udhcpd.leases\n" "$IF" > /tmp/gb-udhcpd.conf
        touch /tmp/gb-udhcpd.leases
        (command -v udhcpd >/dev/null 2>&1 && udhcpd /tmp/gb-udhcpd.conf || busybox udhcpd /tmp/gb-udhcpd.conf) && IP=192.168.77.1
      fi
      echo "$IP" > "$GBDIR/netmidi.ip"
      echo "Netzwerk: $IF  R36S-Adresse: $IP (ohne DHCP: PC bekommt nach ~1 min eine 169.254.x.x-Adresse)"
      IPINFO=" $IP:5004"
    fi
    say "OK: PC-Modus aktiv: ${MIDIOK:-kein MIDI}${IPINFO} ${AUDOK}"
    echo "R36S mit dem PC verbinden (am besten USB-A-auf-USB-C-Kabel)"
  else
    cd /; teardown; setrole host
    say "FEHLER: Gadget-Aktivierung fehlgeschlagen (UDC belegt?)"; pause 5; exit 1
  fi
  ;;
esac
echo "--- UDC: $(ls /sys/class/udc 2>/dev/null)  Zustand: $(cat /sys/class/udc/*/state 2>/dev/null)"
} >> "$LOG" 2>&1
RC=$?
pause 3
exit $RC
