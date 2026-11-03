#!/bin/bash
# Groovebox-OS: USB-Modus des OTG-Ports (gleiche Schnittstelle wie die ArkOS-Version)
#   pc-modus.sh aus            -> USB-Host (S-1, Keyboards, USB-DAC)
#   pc-modus.sh an             -> am PC: MIDI + Netzwerk (SSH, Browser-Upload)
#   pc-modus.sh an audio       -> am PC: Audio-Interface + MIDI + Netzwerk
#   pc-modus.sh laufwerk       -> am PC: Datenpartition GROOVEBOX als USB-Laufwerk + Netzwerk
#   pc-modus.sh laufwerk-aus   -> Laufwerk zurueck an die Groovebox, wieder "an audio"
DIR=/opt/groovebox
LOG=$DIR/pc-modus.log
G=/sys/kernel/config/usb_gadget/groovebox
IP=192.168.77.1
exec >"$LOG" 2>&1
echo "pc-modus $* ($(date))"

role() { for r in /sys/class/usb_role/*/role; do [ -f "$r" ] && echo "$1" > "$r" 2>/dev/null; done; }

teardown() {
    pkill -f "groovebox --dhcpd" 2>/dev/null
    [ -d $G ] || return 0
    echo "" > $G/UDC 2>/dev/null
    rm -f $G/os_desc/c.1
    for l in $G/configs/c.1/*.usb0; do [ -L "$l" ] && rm -f "$l"; done
    rmdir $G/configs/c.1/strings/0x409 $G/configs/c.1 2>/dev/null
    for f in $G/functions/*; do rmdir "$f" 2>/dev/null; done
    rmdir $G/strings/0x409 $G 2>/dev/null
}

setup() {   # $1 = audio (0/1), $2 = laufwerk-Geraet oder leer
    mountpoint -q /sys/kernel/config || mount -t configfs none /sys/kernel/config
    mkdir -p $G && cd $G || { echo "FEHLER: configfs nicht verfuegbar"; return 1; }
    echo 0x1d6b > idVendor; echo 0x0104 > idProduct; echo 0x0100 > bcdDevice; echo 0x0200 > bcdUSB
    echo 0xEF > bDeviceClass; echo 0x02 > bDeviceSubClass; echo 0x01 > bDeviceProtocol   # Composite mit IAD
    mkdir -p strings/0x409
    echo "Groovebox" > strings/0x409/manufacturer
    echo "R36S Groovebox" > strings/0x409/product
    cat /etc/machine-id 2>/dev/null | cut -c1-12 > strings/0x409/serialnumber
    mkdir -p configs/c.1/strings/0x409
    echo "Groovebox" > configs/c.1/strings/0x409/configuration
    echo 500 > configs/c.1/MaxPower

    # Netzwerk (RNDIS, Windows erkennt es ueber os_desc ohne Treiber) - muss die erste Funktion sein
    mkdir -p functions/rndis.usb0
    echo 1 > os_desc/use; echo 0xcd > os_desc/b_vendor_code; echo MSFT100 > os_desc/qw_sign
    echo RNDIS > functions/rndis.usb0/os_desc/interface.rndis/compatible_id
    echo 5162001 > functions/rndis.usb0/os_desc/interface.rndis/sub_compatible_id
    ln -s functions/rndis.usb0 configs/c.1/
    ln -s configs/c.1 os_desc/

    if [ -n "$2" ]; then
        mkdir -p functions/mass_storage.usb0
        echo 1 > functions/mass_storage.usb0/stall
        echo 1 > functions/mass_storage.usb0/lun.0/removable
        echo "GROOVEBOX" > functions/mass_storage.usb0/lun.0/inquiry_string 2>/dev/null
        echo "$2" > functions/mass_storage.usb0/lun.0/file
        ln -s functions/mass_storage.usb0 configs/c.1/
    else
        mkdir -p functions/midi.usb0
        echo "R36S Groovebox" > functions/midi.usb0/id
        echo 1 > functions/midi.usb0/in_ports; echo 1 > functions/midi.usb0/out_ports
        ln -s functions/midi.usb0 configs/c.1/
        if [ "$1" = 1 ]; then
            mkdir -p functions/uac2.usb0
            U=functions/uac2.usb0
            echo 3 > $U/p_chmask; echo 44100 > $U/p_srate; echo 2 > $U/p_ssize   # Groovebox -> PC
            echo 3 > $U/c_chmask; echo 44100 > $U/c_srate; echo 2 > $U/c_ssize   # PC -> Groovebox
            echo "R36S Groovebox" > $U/function_name 2>/dev/null
            ln -s $U configs/c.1/
        fi
    fi
    role device
    sleep 0.3
    UDC=$(ls /sys/class/udc | head -1)
    [ -n "$UDC" ] || { echo "FEHLER: kein USB-Geraete-Controller (udc)"; return 1; }
    echo "$UDC" > UDC || { echo "FEHLER: USB-Gadget laesst sich nicht aktivieren"; return 1; }
    for i in $(seq 20); do ip link show usb0 >/dev/null 2>&1 && break; sleep 0.1; done
    ip link set usb0 up; ip addr flush dev usb0; ip addr add $IP/24 dev usb0
    setsid $DIR/groovebox --dhcpd usb0 >/dev/null 2>&1 < /dev/null &
    echo "$IP" > $DIR/netmidi.ip
    return 0
}

case "$1 $2" in
    "aus "*|"aus nosleep")
        teardown; role host; rm -f $DIR/netmidi.ip
        echo "OK: USB-HOST (GERAETE AM R36S)" ;;
    "laufwerk"*)
        [ "$1" = "laufwerk-aus" ] && {
            teardown
            DEV=$(blkid -L GROOVEBOX)
            fsck.vfat -a "$DEV" >/dev/null 2>&1
            mountpoint -q /data || mount -t vfat -o rw,noatime,flush,umask=000,utf8 "$DEV" /data
            setup 1 "" && echo "OK: LAUFWERK ZURUECK, AUDIO + MIDI AKTIV"
            exit 0
        }
        DEV=$(blkid -L GROOVEBOX)
        [ -n "$DEV" ] || { echo "FEHLER: Datenpartition fehlt"; exit 1; }
        sync
        teardown
        umount /data || { echo "FEHLER: /data wird benutzt"; setup 1 ""; exit 1; }
        setup 0 "$DEV" && echo "OK: USB-LAUFWERK GROOVEBOX AM PC" ;;
    "an audio")
        teardown; setup 1 "" && echo "OK: AUDIO + MIDI + NETZ ($IP)" ;;
    *)
        teardown; setup 0 "" && echo "OK: MIDI + NETZ ($IP)" ;;
esac
