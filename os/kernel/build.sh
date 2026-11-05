#!/bin/bash
# Kernel fuer das Groovebox-OS: Linux (mainline) + ROCKNIX-Patches fuer RK3326/R36S
# + USB-Gadget-Audio/MIDI. Ergebnis: os/out/kernel/{Image, rk3326-groovebox-r36s.dtb, modules/}
#
#   os/kernel/build.sh            (braucht aarch64-linux-gnu-gcc, bc, bison, flex, dtc, python3-fdt)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OS="$(dirname "$HERE")"
WORK="${WORK:-$OS/work}"
OUT="$OS/out/kernel"
source "$OS/versions.env"
JOBS="${JOBS:-$(nproc)}"
export ARCH=arm64
[ "$(uname -m)" = aarch64 ] && export CROSS_COMPILE= || export CROSS_COMPILE=aarch64-linux-gnu-

mkdir -p "$WORK" "$OUT"
cd "$WORK"

# --- Quellen
[ -d linux-$LINUX_VERSION ] || git clone -q --depth 1 --branch v$LINUX_VERSION https://github.com/gregkh/linux.git linux-$LINUX_VERSION
if [ ! -d rocknix ]; then
    git clone -q --filter=blob:none --no-checkout https://github.com/ROCKNIX/distribution.git rocknix
    git -C rocknix sparse-checkout set projects/ROCKNIX/devices/RK3326 projects/ROCKNIX/packages/linux/patches \
        projects/ROCKNIX/packages/linux-drivers/generic-dsi
    git -C rocknix checkout -q $ROCKNIX_COMMIT
fi
if [ ! -d rocknix-joypad ]; then
    git init -q rocknix-joypad
    git -C rocknix-joypad fetch -q --depth 1 https://github.com/hackbar/rocknix-joypad.git $JOYPAD_COMMIT
    git -C rocknix-joypad checkout -q FETCH_HEAD
fi
RN=$WORK/rocknix/projects/ROCKNIX
K=$WORK/linux-$LINUX_VERSION

# --- Patches (einmalig)
if [ ! -f $K/.groovebox-patched ]; then
    cd $K
    for p in $RN/packages/linux/patches/mainline/*.patch \
             $RN/devices/RK3326/patches/linux/*.patch \
             $RN/packages/linux/patches/mainline-rockchip/*.patch \
             $RN/packages/linux/patches/7.0/9998-*.patch; do
        echo ">> $(basename $p)"
        patch -p1 -N -s < "$p" || { echo "!! Patch fehlgeschlagen: $p"; exit 1; }
    done
    # generischer DSI-Panel-Treiber (Init-Sequenz steht im Device-Tree)
    cp $RN/packages/linux-drivers/generic-dsi/sources/panel-generic-dsi.c drivers/gpu/drm/panel/
    echo "obj-y += panel-generic-dsi.o" >> drivers/gpu/drm/panel/Makefile
    # Device-Trees von ROCKNIX + unser R36S-Groovebox-Tree
    cp -r $RN/devices/RK3326/linux/dts/rockchip/. arch/arm64/boot/dts/rockchip/
    touch .groovebox-patched
    cd "$WORK"
fi
cp "$OS/dts/"*.dts $K/arch/arm64/boot/dts/rockchip/
grep -q groovebox $K/arch/arm64/boot/dts/rockchip/Makefile || \
    echo 'dtb-$(CONFIG_ARCH_ROCKCHIP) += rk3326-groovebox-r36s.dtb' >> $K/arch/arm64/boot/dts/rockchip/Makefile

# --- Konfiguration: ROCKNIX-RK3326 + Groovebox-Fragment
cd $K
cp $RN/devices/RK3326/linux/linux.aarch64.conf .config
./scripts/kconfig/merge_config.sh -m .config "$HERE/groovebox.config" >/dev/null
make olddefconfig >/dev/null
for o in USB_CONFIGFS_F_UAC2 USB_CONFIGFS_F_UAC1 USB_CONFIGFS_F_MIDI USB_CONFIGFS_MASS_STORAGE USB_CONFIGFS_RNDIS USB_CONFIGFS_NCM; do
    grep -q "^CONFIG_$o=y" .config || { echo "!! CONFIG_$o fehlt"; exit 1; }
done

# --- Bauen
make -j$JOBS Image modules dtbs 2>&1 | grep -E "error|warning: .*undefined|^  (LD|OBJCOPY) +arch" || true
[ -f arch/arm64/boot/Image ] || { echo "!! Kernel-Build fehlgeschlagen"; make -j1 Image 2>&1 | grep -B5 -m3 error; exit 1; }
make -C $K M=$WORK/rocknix-joypad DEVICE=RK3326 -j$JOBS modules >/dev/null
ls $WORK/rocknix-joypad/*.ko

# --- Device-Tree mit dem Display aus der ArkOS-DTB
DTB=arch/arm64/boot/dts/rockchip/rk3326-groovebox-r36s.dtb
[ -f $DTB ] || make rockchip/rk3326-groovebox-r36s.dtb
make rockchip/rk3326-gameconsole-r36s.dtb >/dev/null   # ROCKNIX-Standard-Display als Ausweich-DTB
cp arch/arm64/boot/dts/rockchip/rk3326-gameconsole-r36s.dtb "$OUT/"
if [ -f "$OS/dts/panel-arkos.dtso" ]; then
    dtc -q -I dts -O dtb -o "$WORK/panel-arkos.dtbo" "$OS/dts/panel-arkos.dtso"
    fdtoverlay -i $DTB -o "$OUT/rk3326-groovebox-r36s.dtb" "$WORK/panel-arkos.dtbo"
else
    cp $DTB "$OUT/rk3326-groovebox-r36s.dtb"
fi

cp arch/arm64/boot/Image "$OUT/Image"
rm -rf "$OUT/modules"
make INSTALL_MOD_PATH="$OUT/modules" INSTALL_MOD_STRIP=1 modules_install >/dev/null
make -C $K M=$WORK/rocknix-joypad DEVICE=RK3326 INSTALL_MOD_PATH="$OUT/modules" INSTALL_MOD_STRIP=1 modules_install >/dev/null
rm -f "$OUT"/modules/lib/modules/*/build "$OUT"/modules/lib/modules/*/source
ls "$OUT"/modules/lib/modules/*/updates/ | grep -q singleadc || { echo "!! Tastentreiber fehlt"; exit 1; }
echo ">> Kernel fertig: $(ls -la $OUT/Image | awk '{print $5}') Bytes, Version $(make -s kernelrelease)"
