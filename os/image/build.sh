#!/bin/bash
# Setzt das SD-Karten-Image zusammen (ohne Loop-Geraete):
#   Sektor 64:   U-Boot (idbloader + uboot.img + trust.img)
#   Partition 1: GBBOOT (FAT32, 128 MB) Kernel, Device-Tree, extlinux.conf
#   Partition 2: GBROOT (ext4) System
#   Partition 3: GROOVEBOX (FAT32) wird beim ersten Start im Restplatz angelegt
# Braucht: mtools, dosfstools, e2fsprogs, fdisk, xz; als root (Dateirechte im ext4)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OS="$(dirname "$HERE")"
O="$OS/out"
IMG="$O/r36s-groovebox-os.img"
K="$O/kernel"
for f in "$O/uboot/uboot.bin" "$O/uboot/boot.scr" "$K/Image" "$K/rk3326-groovebox-r36s.dtb" "$O/rootfs/etc/groovebox-os"; do
    [ -e "$f" ] || { echo "!! fehlt: $f"; exit 1; }
done

BOOT_MB=128
ROOT_MB=$(( $(du -sm "$O/rootfs" | cut -f1) * 13 / 10 + 256 ))
START=32768                       # 16 MiB
BOOT_S=$((BOOT_MB * 2048))
ROOT_S=$((ROOT_MB * 2048))
TOTAL_S=$((START + BOOT_S + ROOT_S + 2048))

rm -f "$IMG" "$IMG.xz"
truncate -s $((TOTAL_S * 512)) "$IMG"
DISKID=47425653                    # feste MBR-Kennung -> root=PARTUUID=47425653-02 (Kernel ohne Initramdisk kennt kein LABEL=)
sfdisk -q "$IMG" <<EOF
label: dos
label-id: 0x$DISKID
start=$START, size=$BOOT_S, type=c, bootable
start=$((START + BOOT_S)), size=$ROOT_S, type=83
EOF

# Bootpartition
B="$O/boot.vfat"
rm -f "$B"
mkfs.vfat -F 32 -n GBBOOT -C "$B" $((BOOT_MB * 1024)) >/dev/null
mkdir -p "$O/bootfiles/extlinux"
# Testphase: Kernelmeldungen auf dem Display (console=tty0), bei einem Absturz 60 s stehen bleiben (panic=60)
cat > "$O/bootfiles/extlinux/extlinux.conf" <<EOF
LABEL groovebox
  LINUX /Image
  FDT /rk3326-groovebox-r36s.dtb
  APPEND root=PARTUUID=$DISKID-02 rootfstype=ext4 rootwait rw console=ttyS2,1500000 console=tty0 loglevel=7 panic=60 consoleblank=0
EOF
cat > "$O/bootfiles/LIESMICH.txt" <<'EOF'
R36S Groovebox OS
Diagnose-Logs liegen nach dem ersten Start auf der Partition GROOVEBOX im Ordner logs/.
Anderes Display testen: in extlinux/extlinux.conf die FDT-Zeile auf
/rk3326-gameconsole-r36s.dtb (ROCKNIX-Standard-Display) aendern.
EOF
export MTOOLS_SKIP_CHECK=1
mcopy -i "$B" "$K/Image" "$K/rk3326-groovebox-r36s.dtb" "$O/bootfiles/LIESMICH.txt" ::
mcopy -i "$B" "$O/uboot/boot.scr" ::
[ -f "$K/rk3326-gameconsole-r36s.dtb" ] && mcopy -i "$B" "$K/rk3326-gameconsole-r36s.dtb" ::
mcopy -i "$B" -s "$O/bootfiles/extlinux" ::

# Systempartition
R="$O/root.ext4"
rm -f "$R"
mkfs.ext4 -q -L GBROOT -O ^metadata_csum_seed -d "$O/rootfs" "$R" ${ROOT_MB}M

dd if="$O/uboot/uboot.bin" of="$IMG" bs=512 seek=64 conv=notrunc status=none
dd if="$B" of="$IMG" bs=512 seek=$START conv=notrunc status=none
dd if="$R" of="$IMG" bs=512 seek=$((START + BOOT_S)) conv=notrunc status=none
rm -f "$B" "$R"
xz -T0 -6 -k "$IMG"
echo ">> Image fertig: $(ls -la "$IMG.xz" | awk '{print $5}') Bytes (entpackt $((TOTAL_S / 2048)) MB)"
