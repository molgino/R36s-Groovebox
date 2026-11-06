#!/bin/bash
# Groovebox-OS: USB-Modus des OTG-Ports (gleiche Schnittstelle wie die ArkOS-Version)
#   pc-modus.sh aus            -> USB-Host (S-1, Keyboards, USB-DAC)
#   pc-modus.sh an             -> am PC: MIDI + Netzwerk (SSH, Browser-Upload)
#   pc-modus.sh an audio       -> am PC: Audio-Interface + MIDI + Netzwerk
#   pc-modus.sh laufwerk       -> am PC: Datenpartition GROOVEBOX als USB-Laufwerk + Netzwerk
#   pc-modus.sh laufwerk-aus   -> Laufwerk zurueck an die Groovebox, wieder "an audio"
# Alles class compliant (keine Treiber am PC). Umschaltbar per Umgebung (setzt die App aus SET, oder config/groovebox.env):
#   GB_USB_NET=ncm|rndis   Netzwerk: NCM (Standard: Windows 11/10 ab 2004, macOS, iPadOS, Linux, Android) oder RNDIS (aeltere Windows 10)
#   GB_UAC=2|1             Audio: UAC2 (Standard, 44,1 + 48 kHz) oder UAC1 (aeltere Geraete/Konsolen; braucht CONFIG_USB_CONFIGFS_F_UAC1)
#   GB_UAC_RATES="44100,48000"   angebotene Abtastraten (der PC waehlt)
#   GB_UAC_CH=2|8|18|24    Kanaele zum PC (UAC2): 1-2 Master, 8 = +FX A/B/C, 18 = +8 Spuren, 24 = Spuren + FX
#   GB_UAC_IN=1|0          0 = nur senden: Host sieht nur einen Eingang (behaelt eigenen Lautsprecher, z. B. iPhone)
#   GB_USB_POWER=<mA>      Strombedarf melden (Standard: eigene Stromversorgung, 2 mA - fuer iPhone/iPad)
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
    echo 0x1d6b > idVendor; echo 0x0104 > idProduct; echo 0x0101 > bcdDevice; echo 0x0200 > bcdUSB
    echo 0xEF > bDeviceClass; echo 0x02 > bDeviceSubClass; echo 0x01 > bDeviceProtocol   # Composite mit IAD
    mkdir -p strings/0x409
    echo "Groovebox" > strings/0x409/manufacturer
    echo "Synth City" > strings/0x409/product
    cat /etc/machine-id 2>/dev/null | cut -c1-12 > strings/0x409/serialnumber
    mkdir -p configs/c.1/strings/0x409
    echo "Groovebox" > configs/c.1/strings/0x409/configuration
    # Eigene Stromversorgung (Akku): kaum Strombedarf melden, sonst lehnen iPhone/iPad das Geraet ab
    # ("Zubehoer benoetigt zu viel Strom"). Ueberschreibbar: GB_USB_POWER=<mA> (dann bus-powered).
    if [ -n "$GB_USB_POWER" ]; then
        echo 0x80 > configs/c.1/bmAttributes; echo "$GB_USB_POWER" > configs/c.1/MaxPower
    else
        echo 0xC0 > configs/c.1/bmAttributes; echo 2 > configs/c.1/MaxPower
    fi

    # Netzwerk - muss die erste Funktion sein (Schnittstelle usb0)
    if [ "${GB_USB_NET:-ncm}" = rndis ]; then   # RNDIS: Windows erkennt es ueber os_desc ohne Treiber
        mkdir -p functions/rndis.usb0
        echo 1 > os_desc/use; echo 0xcd > os_desc/b_vendor_code; echo MSFT100 > os_desc/qw_sign
        echo RNDIS > functions/rndis.usb0/os_desc/interface.rndis/compatible_id
        echo 5162001 > functions/rndis.usb0/os_desc/interface.rndis/sub_compatible_id
        ln -s functions/rndis.usb0 configs/c.1/
        ln -s configs/c.1 os_desc/
        echo "Netz: RNDIS"
    else                                         # CDC-NCM: Standardklasse, ueberall ohne Treiber
        mkdir -p functions/ncm.usb0
        ln -s functions/ncm.usb0 configs/c.1/
        echo "Netz: NCM"
    fi

    if [ -n "$2" ]; then
        mkdir -p functions/mass_storage.usb0
        echo 1 > functions/mass_storage.usb0/stall
        echo 1 > functions/mass_storage.usb0/lun.0/removable
        echo "GROOVEBOX" > functions/mass_storage.usb0/lun.0/inquiry_string 2>/dev/null
        echo "$2" > functions/mass_storage.usb0/lun.0/file
        ln -s functions/mass_storage.usb0 configs/c.1/
    else
        mkdir -p functions/midi.usb0
        echo "Synth City" > functions/midi.usb0/id
        echo "Synth City" > functions/midi.usb0/interface_string 2>/dev/null   # Name am PC statt "MIDI function"
        echo 1 > functions/midi.usb0/in_ports; echo 1 > functions/midi.usb0/out_ports
        ln -s functions/midi.usb0 configs/c.1/
        if [ "$1" = 1 ]; then
            RATES=${GB_UAC_RATES:-44100,48000}
            SS=${GB_UAC_SSIZE:-3}   # Bytes je Sample: 3 = 24 Bit (Standard), 2 = 16 Bit
            U=""
            if [ "${GB_UAC:-2}" = 1 ]; then          # UAC1: auch fuer Geraete ohne UAC2 (Windows 7/8, Konsolen, alte Recorder)
                if mkdir functions/uac1.usb0 2>/dev/null; then
                    U=functions/uac1.usb0
                    CIN=3; [ "${GB_UAC_IN:-1}" = 0 ] && CIN=0
                    echo 3 > $U/p_chmask; echo $CIN > $U/c_chmask; echo $SS > $U/p_ssize 2>/dev/null || echo 2 > $U/p_ssize; echo $SS > $U/c_ssize 2>/dev/null || echo 2 > $U/c_ssize
                    echo $RATES > $U/p_srate 2>/dev/null || echo 44100 > $U/p_srate
                    echo $RATES > $U/c_srate 2>/dev/null || echo 44100 > $U/c_srate
                    echo "Synth City" > $U/function_name 2>/dev/null
                    echo ${GB_UAC_REQ:-4} > $U/req_number 2>/dev/null
                    echo "UAC1: p_srate=$(cat $U/p_srate) c_srate=$(cat $U/c_srate) ssize=$(cat $U/p_ssize)"
                else
                    echo "WARNUNG: UAC1 fehlt im Kernel - nehme UAC2"
                fi
            fi
            if [ -z "$U" ]; then
                mkdir -p functions/uac2.usb0
                U=functions/uac2.usb0
                CH=${GB_UAC_CH:-2}; case "$CH" in 2|8|18|24) ;; *) CH=2 ;; esac
                PSS=$SS
                # 24 Kanaele mit 24 Bit = 936-Byte-Pakete alle 250 us: am R36S (dwc2) stockte es nach einer Minute (Broken pipe,
                # Audio-Aussetzer); 18 Kanaele (702 Byte) liefen sauber -> bei 24 Kanaelen 16 Bit (624 Byte), ausser GB_UAC_SSIZE ist gesetzt
                [ "$CH" -ge 24 ] && [ -z "$GB_UAC_SSIZE" ] && PSS=2
                echo $(( (1 << CH) - 1 )) > $U/p_chmask; echo $PSS > $U/p_ssize 2>/dev/null || echo 2 > $U/p_ssize   # Groovebox -> PC
                # Paketabstand: so gross wie moeglich (dwc2 mag 1 ms), aber ein Paket (max. 1024 Byte) muss alle Kanaele fassen:
                # Bytes = Kanaele x Bytes/Sample x (48 kHz-Samples je Abstand + 1); Abstand = 125 us x 2^(bInterval-1)
                PB=4; SSZ=$(cat $U/p_ssize 2>/dev/null || echo $PSS)
                while [ $PB -gt 1 ] && [ $(( CH * SSZ * (6 * (1 << (PB - 1)) + 1) )) -gt 1024 ]; do PB=$((PB - 1)); done
                CIN=3; [ "${GB_UAC_IN:-1}" = 0 ] && CIN=0   # 0: kein Ausgang am Host -> Handy/PC behaelt Lautsprecher
                echo $CIN > $U/c_chmask; echo $SS > $U/c_ssize 2>/dev/null || echo 2 > $U/c_ssize   # PC -> Groovebox
                echo $RATES > $U/p_srate 2>/dev/null || echo 44100 > $U/p_srate   # mehrere Raten: Kernel >= 5.18
                echo $RATES > $U/c_srate 2>/dev/null || echo 44100 > $U/c_srate
                echo "Synth City" > $U/function_name 2>/dev/null
                # dwc2 (RK3326) schafft isochrone Pakete jede 125 us oft nicht -> Standard: alle 1 ms (bInterval 4) und mehr Anfragen.
                # Zum Testen in config/groovebox.env: GB_UAC_BINT=1..4, GB_UAC_REQ=2..8
                echo ${GB_UAC_BINT:-$PB} > $U/p_hs_bint 2>/dev/null; echo ${GB_UAC_BINT:-4} > $U/c_hs_bint 2>/dev/null
                [ "$CH" -gt 2 ] && echo "UAC2: $CH Kanaele zum PC (p_chmask $(cat $U/p_chmask), Paketabstand bInterval $(cat $U/p_hs_bint 2>/dev/null))"
                echo ${GB_UAC_REQ:-4} > $U/req_number 2>/dev/null
                echo "UAC2: p_srate=$(cat $U/p_srate) c_srate=$(cat $U/c_srate) ssize=$(cat $U/p_ssize) p_hs_bint=$(cat $U/p_hs_bint 2>/dev/null) c_hs_bint=$(cat $U/c_hs_bint 2>/dev/null) req_number=$(cat $U/req_number 2>/dev/null)"
            fi
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
