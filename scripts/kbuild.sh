#!/bin/sh
# Runs inside the Ubuntu 14.04 chroot (tools/trusty) with the project at /minos.
# The 2.6.31 kernel needs gcc 4.7 and make 3.81 of that era.
# Usage: kbuild.sh kernel <defconfig>
set -e
M="make CROSS_COMPILE=arm-linux-gnueabi-"
case $1 in
kernel)
	cd /minos/build/src/linux-2.6.31
	cp -r /minos/kernel/* include/          # headers missing from Amazon's tarball
	$M ARCH=arm $2
	cat /minos/kernel.config >> .config
	yes "" | $M ARCH=arm oldconfig >/dev/null
	$M ARCH=arm -j$(nproc) uImage modules
	rm -rf /minos/build/modules
	$M ARCH=arm INSTALL_MOD_PATH=/minos/build/modules modules_install
	cp arch/arm/boot/uImage /minos/build/uImage ;;
esac
