# Groovebox-OS: U-Boot-Startskript (wird zu boot.scr). Laedt extlinux/extlinux.conf von der Bootpartition.
echo "R36S Groovebox OS"
setenv kernel_addr_r "0x09000000"
setenv fdtoverlay_addr_r "0x08000000"
sysboot ${devtype} ${devnum}:${distro_bootpart} any 0x100000 /extlinux/extlinux.conf
