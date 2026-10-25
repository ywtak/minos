#!/bin/sh
# minios flasher. Runs ON THE KINDLE, from the release folder copied to the
# USB drive (/mnt/us/minios-flash). See FLASHING.md before using it.
#
#   sh flash.sh install   minios system on p1, diags system wiped
#                         (stock over SSH, or minios via DO_INSTALL = update)
#   sh flash.sh restore   Amazon's system and diags back from backup/
#                         (minios via DO_RESTORE)
#
# Amazon's kernel and bootloader are never written. Offsets come from
# layout.env. Every write is read back and compared.
# Set YES=1 to skip the typed confirmation (rc.local does this).

case $0 in */*) DIR=${DIR:-${0%/*}} ;; *) DIR=${DIR:-.} ;; esac
DIR=$(cd "$DIR" && pwd)                   # builtins only: no dirname on minios
MODE=$1
DISK=/dev/mmcblk0

die() { echo "flash: $*" >&2; exit 1; }

# Re-run from RAM with our own busybox, so overwriting the running root
# filesystem can never pull a tool out from under us.
# Only the bundled busybox is used, so this works from any system.
if [ -z "$IN_RAM" ]; then
	B="$DIR/busybox"
	T=$("$B" awk '$3=="tmpfs"{print $2; exit}' /proc/mounts)
	[ -n "$T" ] || die "no tmpfs mount found"
	T=$T/minios-flash.$$
	"$B" mkdir -p $T/bin && "$B" cp "$B" "$DIR/flash.sh" $T/ || die "copy to RAM failed"
	"$B" chmod +x $T/busybox
	for a in sh awk cat cut dd echo head od printf sha256sum sleep sync tr wc reboot; do
		"$B" ln -sf $T/busybox $T/bin/$a
	done
	IN_RAM=1 DIR="$DIR" PATH=$T/bin exec $T/busybox sh $T/flash.sh "$@"
fi
[ -f "$DIR/layout.env" ] || die "layout.env missing"
. "$DIR/layout.env"

size()  { wc -c < "$1" | tr -d ' '; }
magic() { dd if="$1" bs=1 skip=${2:-0} count=4 2>/dev/null | od -An -tx1 | tr -d ' \n'; }

confirm() {
	[ "$YES" = 1 ] && return
	echo "$1"
	printf 'Type FLASH to continue: '
	read -r ans
	[ "$ans" = FLASH ] || die "cancelled"
}

# read DEV OFFSET LEN: the bytes, for comparing
region() { dd if="$1" bs=512 skip=$(($2 / 512)) count=$(( ($3 + 511) / 512 )) 2>/dev/null | head -c $3; }

# put FILE DEV OFFSET: write FILE at byte OFFSET of DEV and read it back
put() {
	n=$(size "$1")
	echo "writing $1 -> $2 @ $3 ($n bytes)"
	dd if="$1" of="$2" bs=512 seek=$(($3 / 512)) conv=fsync 2>/dev/null || die "write failed"
	[ "$(sha256sum < "$1")" = "$(region "$2" $3 $n | sha256sum)" ] ||
		die "READ-BACK MISMATCH on $2 @ $3 - do not reboot, ask for help"
}

# wipe DEV OFFSET LEN: zero LEN bytes at OFFSET and read them back
wipe() {
	echo "wiping $1 @ $2 ($3 bytes)"
	dd if=/dev/zero of="$1" bs=512 seek=$(($2 / 512)) count=$(($3 / 512)) conv=fsync 2>/dev/null || die "wipe failed"
	[ "$(head -c $3 /dev/zero | sha256sum)" = "$(region "$1" $2 $3 | sha256sum)" ] ||
		die "READ-BACK MISMATCH wiping $1 @ $2 - do not reboot, ask for help"
}

cd "$DIR" || die "no $DIR"
[ "$(magic $DISK $MAIN_KERNEL_OFFSET)" = 27051956 ] || die "no kernel at MAIN_KERNEL_OFFSET: layout.env does not match this Kindle"
case "$MODE" in
install)
	sha256sum -c SHA256SUMS || die "checksum mismatch, copy the release again"
	[ $(size rootfs.img) -le $ROOTFS_MAX ] || die "rootfs.img is larger than p1"
	confirm "Install replaces the Amazon system with minios and wipes the diags system."
	put rootfs.img $ROOTFS_PART 0
	m=$(magic $DISK $DIAGS_KERNEL_OFFSET)
	[ "$m" = 27051956 -o "$m" = 00000000 ] || die "unexpected data at DIAGS_KERNEL_OFFSET, not wiping"
	wipe $DISK $DIAGS_KERNEL_OFFSET $DIAGS_KERNEL_LEN
	wipe $DIAGS_PART 0 $DIAGS_PART_LEN
	;;
restore)
	cd backup 2>/dev/null && sha256sum -c SHA256SUMS ||
		die "backup/ needs p1.img, p2.img, diags-kernel.img and their SHA256SUMS (FLASHING.md)"
	confirm "Restore writes Amazon's system and diags back."
	put diags-kernel.img $DISK $DIAGS_KERNEL_OFFSET
	put p2.img $DIAGS_PART 0
	put p1.img $ROOTFS_PART 0
	;;
*)
	die "usage: flash.sh install|restore"
	;;
esac
sync
echo "flash: $MODE done, rebooting"
sleep 2
reboot -f
