#!/bin/sh
# Run on the jailbroken STOCK Kindle (over SSH). Read-only: it changes nothing.
# Writes /mnt/us/minios-facts.txt and /mnt/us/layout.env.suggested.
# Send minios-facts.txt back so NOTES.md can be updated with real values.

OUT=/mnt/us/minios-facts.txt
SUG=/mnt/us/layout.env.suggested
DISK=/dev/mmcblk0
BB=$(dirname "$0")/busybox                  # stock 4.1.4 busybox has no od
command -v od >/dev/null || od() { "$BB" od "$@"; }

sec() { echo; echo "== $1 =="; }
peek() { dd if=$DISK bs=1 skip=$1 count=$2 2>/dev/null | od -An -tx1 | tr -d ' \n'; }
word() { dd if=$DISK bs=1 skip=$1 count=4 2>/dev/null | od -An -tu4 | tr -d ' \n'; }   # little-endian

{
sec uname;            uname -a
sec cmdline;          cat /proc/cmdline
sec cpuinfo;          cat /proc/cpuinfo
sec meminfo;          head -3 /proc/meminfo
sec partitions;       cat /proc/partitions
sec mounts;           cat /proc/mounts
sec fdisk;            fdisk -l $DISK 2>&1
sec modules;          cat /proc/modules
sec module-loading;   grep -rhs -e modprobe -e insmod /etc/init.d /etc/upstart /etc/rc*.d 2>/dev/null | sort -u
sec usb-gadget;       grep -rhs -e g_file_storage -e file_storage /etc /usr/sbin 2>/dev/null | sort -u | head -20
sec fb0;              for f in /sys/class/graphics/fb0/*; do [ -f $f ] && echo "$(basename $f): $(head -1 $f 2>/dev/null)"; done
sec input-devices;    cat /proc/bus/input/devices
sec power-supply;     for f in $(find /sys -name online 2>/dev/null) $(find /sys -name '*connected*' 2>/dev/null); do echo "$f = $(cat $f 2>/dev/null)"; done
sec sysrq;            ls -l /proc/sysrq-trigger 2>&1
sec battery;          for f in $(find /sys/devices/system -name 'battery_id*' 2>/dev/null); do echo "$(basename $f): $(cat $f)"; done
sec board;            for f in board_id usid panel_id bootmode; do echo "$f: $(cat /proc/$f 2>/dev/null)"; done
sec idme;             idme -d 2>&1 | head -20
sec uboot-env;        fw_printenv 2>&1 | head -40
sec ivt
IVT=$(peek 1024 4)
echo "bytes at 0x400: $IVT   (i.MX IVT header starts with d1)"
UBOOT_LEN=0
if [ "${IVT%??????}" = "d1" ]; then
	SELF=$(word $((1024 + 20))); BOOTDATA=$(word $((1024 + 16)))
	UBOOT_LEN=$(word $((1024 + BOOTDATA - SELF + 4)))
	echo "IVT self=$SELF boot_data=$BOOTDATA -> stock u-boot image length $UBOOT_LEN bytes from disk offset 0"
fi
sec uimage-scan
i=0
while [ $i -lt 4096 ]; do                    # first 16 MiB, 4 KiB steps
	if [ "$(peek $((i * 4096)) 4)" = "27051956" ]; then
		size=$(dd if=$DISK bs=1 skip=$((i * 4096 + 12)) count=4 2>/dev/null | od -An -tu1 | awk '{print $1*16777216+$2*65536+$3*256+$4}')
		name=$(dd if=$DISK bs=1 skip=$((i * 4096 + 32)) count=32 2>/dev/null | tr -d '\0')
		echo "uImage at $((i * 4096)) size $size name '$name'"
	fi
	i=$((i + 1))
done
} > $OUT 2>&1

# Suggest a layout: first uImage = main kernel; its slot ends at the next uImage.
UBOOT_LEN=$(sed -n 's/.*stock u-boot image length \([0-9]*\).*/\1/p' $OUT)
set -- $(grep '^uImage at' $OUT | awk '{print $3}')
K=$1; N=$2
P1=$(awk '$4=="mmcblk0p1"{print $3}' /proc/partitions)
{
	echo "# Suggested by discover.sh on $(date). CHECK every value against minios-facts.txt."
	echo "KERNEL_OFFSET=${K:-UNKNOWN}"
	echo "KERNEL_MAX=${N:+$((N - K))}"
	echo "ROOTFS_PART=/dev/mmcblk0p1"
	echo "ROOTFS_MAX=$(( ${P1:-0} * 1024 ))"
	echo "UBOOT_OFFSET=1024"
	echo "UBOOT_MAX=${UBOOT_LEN:+$((UBOOT_LEN - 1024))}"
} > $SUG

echo "Wrote $OUT and $SUG"
