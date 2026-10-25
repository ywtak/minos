/* USB drive mode: while the cable is in, the PC owns the userstore
 * partition (g_file_storage); we unmount it first and remount after.
 * The FAT starts 8 KiB into the partition, so Linux mounts it through a
 * loop device; detaching it drops any cache the PC's writes made stale. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <unistd.h>
#include "minios.h"

int usb_online(void)
{
	static const char *paths[] = USB_ONLINE_PATHS;
	const char **p;

	if (getenv("MINIOS_NO_USB"))           /* testing over USB networking */
		return 0;
	for (p = paths; *p; p++) {
		FILE *f = fopen(*p, "r");
		if (f) {
			int c = fgetc(f);
			fclose(f);
			return c == '1';
		}
	}
	return 0;
}

void usb_export(void)
{
	sync();
	if (umount(USERSTORE_MNT))
		logmsg("umount userstore failed; exporting anyway");
	system(USERSTORE_DETACH);
	if (system(USB_EXPORT_CMD))
		logmsg("usb export failed: %s", USB_EXPORT_CMD);
}

void usb_unexport(void)
{
	system(USB_UNEXPORT_CMD " 2>/dev/null");   /* also run at boot, when nothing is loaded */
	if (system(USERSTORE_ATTACH))
		logmsg("losetup failed: %s", USERSTORE_ATTACH);
	if (mount(USERSTORE_LOOP, USERSTORE_MNT, "vfat", 0, "utf8,shortname=mixed"))
		logmsg("remount userstore failed");
}
