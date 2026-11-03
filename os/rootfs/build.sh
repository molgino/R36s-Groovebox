#!/bin/bash
# Root-Dateisystem fuer das Groovebox-OS: Debian (minimal) + SDL2/ALSA/Mesa + Groovebox als Dienst.
# Laeuft als root auf einem arm64-Rechner (z. B. GitHub-Runner ubuntu-24.04-arm), braucht mmdebstrap.
#   sudo os/rootfs/build.sh <groovebox-binary>
# Ergebnis: os/out/rootfs/ (Verzeichnis)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OS="$(dirname "$HERE")"
REPO="$(dirname "$OS")"
source "$OS/versions.env"
BIN="${1:-$REPO/groovebox}"
ROOT="$OS/out/rootfs"
KMOD="$OS/out/kernel/modules/lib/modules"
[ -x "$BIN" ] || { echo "!! Groovebox-Programm fehlt: $BIN"; exit 1; }
[ -d "$KMOD" ] || { echo "!! Kernel-Module fehlen: $KMOD"; exit 1; }

PKGS="systemd,systemd-sysv,udev,kmod,dbus,iproute2,procps,sudo,less,nano,coreutils,util-linux,util-linux-extra,fdisk,dosfstools,e2fsprogs"
PKGS+=",libsdl2-2.0-0,libasound2t64,alsa-utils,libgl1-mesa-dri,libegl1,libgles2,libgbm1,libdrm2"
PKGS+=",dropbear,openssh-sftp-server,ca-certificates"

rm -rf "$ROOT"
mmdebstrap --variant=minbase --include="$PKGS" --components=main \
    --dpkgopt='path-exclude=/usr/share/doc/*' --dpkgopt='path-exclude=/usr/share/man/*' \
    --dpkgopt='path-exclude=/usr/share/locale/*' --dpkgopt='path-include=/usr/share/locale/locale.alias' \
    "$DEBIAN_SUITE" "$ROOT" http://deb.debian.org/debian

# Kernel-Module
mkdir -p "$ROOT/usr/lib/modules"
cp -r --preserve=mode,timestamps "$KMOD"/. "$ROOT/usr/lib/modules/"
chown -R root:root "$ROOT/usr/lib/modules"
KVER=$(ls "$KMOD" | head -1)
depmod -b "$ROOT" "$KVER" 2>/dev/null || chroot "$ROOT" depmod -a "$KVER"

# eigene Dateien
cp -r --preserve=mode,timestamps "$HERE/overlay/." "$ROOT/"
chown -R root:root "$ROOT/opt/groovebox" "$ROOT/usr/local/sbin" "$ROOT/etc/systemd"
install -m 755 "$BIN" "$ROOT/opt/groovebox/groovebox"
mkdir -p "$ROOT/opt/groovebox/defaults"
cp -r "$REPO/arkos/groovebox/samples" "$ROOT/opt/groovebox/defaults/samples"
cd "$ROOT/opt/groovebox"
for d in samples projects presets; do ln -sfn /data/$d $d; done
ln -sfn /data/config/settings.cfg settings.cfg
ln -sfn /data/config/input.cfg input.cfg
ln -sfn /data/update update
echo "$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null) $(date +%Y-%m-%d)" > VERSION
cd - >/dev/null

echo "Groovebox-OS $(date +%Y-%m-%d) ($(git -C "$REPO" rev-parse --short HEAD 2>/dev/null))" > "$ROOT/etc/groovebox-os"
echo r36s-groovebox > "$ROOT/etc/hostname"
printf '127.0.0.1\tlocalhost\n127.0.1.1\tr36s-groovebox\n' > "$ROOT/etc/hosts"
cat > "$ROOT/etc/fstab" <<'EOF'
LABEL=GBROOT  /      ext4  defaults,noatime,commit=30  0 1
LABEL=GBBOOT  /boot  vfat  defaults,noauto              0 0
EOF
mkdir -p "$ROOT/boot" "$ROOT/data"

chroot "$ROOT" /bin/bash -e <<'EOF'
useradd -m -s /bin/bash -G sudo,audio,video,input ark || true
echo ark:ark | chpasswd
echo root:ark | chpasswd
systemctl enable groovebox-data.service groovebox.service
systemctl mask getty@tty1.service apt-daily.timer apt-daily-upgrade.timer e2scrub_all.timer fstrim.timer 2>/dev/null || true
systemctl set-default multi-user.target
rm -f /etc/machine-id; touch /etc/machine-id
apt-get clean
rm -rf /var/lib/apt/lists/*
EOF
echo ">> Rootfs fertig: $(du -sh "$ROOT" | cut -f1)"
