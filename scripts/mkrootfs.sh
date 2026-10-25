#!/bin/sh
# Build the minios root filesystem (ext3) without root rights.
#   scripts/mkrootfs.sh build/rootfs.img
# Contents: /sbin/init = minios, busybox for the few shell helpers,
# Amazon's kernel modules (build/stock-modules, `make stock-modules`), rootfs/etc.
set -e
OUT=$1
SIZE_KB=${ROOTFS_KB:-98304}      # must stay below ROOTFS_MAX in layout.env
R=build/rootfs

rm -rf $R
mkdir -p $R/bin $R/sbin $R/etc $R/lib/modules $R/proc $R/sys $R/dev $R/tmp $R/mnt/us
cp build/minios $R/sbin/init
cp third_party/busybox $R/bin/busybox
for a in sh awk cat chmod cp cut dd dirname echo grep head ln losetup ls mkdir mount \
         od printf rm sleep sync tr umount wc sha256sum reboot; do
	ln -s busybox $R/bin/$a
done
for a in mdev insmod rmmod; do
	ln -s ../bin/busybox $R/sbin/$a
done
# flat /lib/modules/NAME.ko, loaded with insmod (rc.local, config.h)
for m in $(grep -v '^#' rootfs/etc/modules | cut -d' ' -f1) g_file_storage; do
	ko=$(find build/stock-modules -name "$m.ko" | head -1)
	[ -n "$ko" ] || { echo "missing stock module $m.ko (make stock-modules)"; exit 1; }
	cp "$ko" $R/lib/modules/
done
cp rootfs/etc/* $R/etc/
chmod +x $R/etc/rc.local

rm -f "$OUT"
genext2fs -b $SIZE_KB -N 4096 -U -d $R -D rootfs/devtable "$OUT"
tune2fs -j "$OUT" >/dev/null           # add a journal: ext3
echo "$OUT: $(du -h "$OUT" | cut -f1)"
