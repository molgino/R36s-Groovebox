#!/bin/bash
# U-Boot fuer den R36S (RK3326) nach ROCKNIX-Rezept: mainline U-Boot + Rockchip-Miniloader/BL31 aus rkbin.
# Ergebnis: os/out/uboot/uboot.bin  (wird ab Sektor 64 auf die Karte geschrieben)
# Braucht einen x86-64-Rechner (rkbin-Werkzeuge), gcc-aarch64-linux-gnu, swig, python3-pyelftools, libssl-dev, libgnutls28-dev
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OS="$(dirname "$HERE")"
WORK="${WORK:-$OS/work}"
OUT="$OS/out/uboot"
source "$OS/versions.env"
mkdir -p "$WORK" "$OUT"
cd "$WORK"

[ -d u-boot-$UBOOT_VERSION ] || git clone -q --depth 1 --branch $UBOOT_VERSION https://github.com/u-boot/u-boot.git u-boot-$UBOOT_VERSION
if [ ! -d rocknix-uboot ]; then
    git clone -q --filter=blob:none --no-checkout https://github.com/ROCKNIX/distribution.git rocknix-uboot
    git -C rocknix-uboot sparse-checkout set projects/ROCKNIX/devices/RK3326/packages/u-boot packages/tools/u-boot/patches
    git -C rocknix-uboot checkout -q $ROCKNIX_COMMIT
fi
if [ ! -d rkbin ]; then
    git clone -q --filter=blob:none --no-checkout https://github.com/rockchip-linux/rkbin.git rkbin
    git -C rkbin sparse-checkout set bin/rk33 tools
    git -C rkbin checkout -q $RKBIN_COMMIT
fi
RU=$WORK/rocknix-uboot/projects/ROCKNIX/devices/RK3326/packages/u-boot
U=$WORK/u-boot-$UBOOT_VERSION

cd $U
if [ ! -f .groovebox-patched ]; then
    cp -r $RU/sources/. .
    for p in $WORK/rocknix-uboot/packages/tools/u-boot/patches/*.patch $RU/patches/*.patch; do
        echo ">> $(basename $p)"; patch -p1 -N -s < "$p"
    done
    touch .groovebox-patched
fi
export ARCH=arm CROSS_COMPILE=aarch64-linux-gnu-
make -s mrproper
make -s rk3326-handheld_defconfig
make -s -j$(nproc) u-boot-dtb.bin

# Rockchip-Verpackung (wie ROCKNIX bootloader/rkhelper)
RK=$WORK/rkbin
newest() { ls $RK/bin/rk33/$1 2>/dev/null | sort -V | tail -1; }
DDR=$(newest 'rk3326_ddr_333MHz_v*.bin')
MINI=$(newest 'rk3326_miniloader_v*.bin')
BL31=$(newest 'rk3326_bl31_v*.elf')
echo ">> rkbin: $(basename "$DDR") $(basename "$MINI") $(basename "$BL31")"
for f in $DDR $MINI $BL31; do [ -f $f ] || { echo "!! fehlt: $f"; ls $RK/bin/rk33 | grep rk3326; exit 1; }; done
rm -f idbloader.img uboot.img trust.img uboot.bin
./tools/mkimage -n px30 -T rksd -d "$DDR" -C bzip2 idbloader.img
cat "$MINI" >> idbloader.img
$RK/tools/loaderimage --pack --uboot u-boot-dtb.bin uboot.img 0x00200000
cat > trust.ini <<EOF
[BL30_OPTION]
SEC=0
[BL31_OPTION]
SEC=1
PATH=$BL31
ADDR=0x00010000
[BL32_OPTION]
SEC=0
[BL33_OPTION]
SEC=0
[OUTPUT]
PATH=trust.img
EOF
$RK/tools/trust_merger --verbose trust.ini >/dev/null
dd if=idbloader.img of=uboot.bin bs=512 seek=0 conv=fsync,notrunc status=none
dd if=uboot.img of=uboot.bin bs=512 seek=16320 conv=fsync,notrunc status=none
dd if=trust.img of=uboot.bin bs=512 seek=24512 conv=fsync,notrunc status=none
cp uboot.bin "$OUT/uboot.bin"
./tools/mkimage -q -A arm64 -T script -C none -d "$HERE/boot.cmd" "$OUT/boot.scr"
echo ">> U-Boot fertig: $(stat -c %s $OUT/uboot.bin) Bytes"
