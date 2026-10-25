/* E-ink output: copy the screen buffer into /dev/fb0 and ask Lab126's
 * eink_fb shim (which owns fb0 on top of mxc_epdc_fb) to refresh the panel.
 * pwrite, not mmap: an mmap'd fb0 uses deferred I/O, which redraws the
 * panel again 250 ms after every write (a second, wasted refresh). */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/fb.h>
#include "minios.h"

static int fd = -1;
static uint8_t *buf;
static int stride, xres, yres;
static off_t offset;

int disp_init(void)
{
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;

	if ((fd = open("/dev/fb0", O_RDWR)) < 0 ||
	    ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f)) {
		logmsg("fb0: %s", strerror(errno));
		return -1;
	}
	logmsg("fb0: %ux%u virt %ux%u bpp %u line %u gray %u rotate %u",
	       v.xres, v.yres, v.xres_virtual, v.yres_virtual, v.bits_per_pixel,
	       f.line_length, v.grayscale, v.rotate);
	if (v.bits_per_pixel != 8) {
		logmsg("fb0: only 8bpp is supported");
		return -1;
	}
	xres = v.xres < SCREEN_W ? v.xres : SCREEN_W;
	yres = v.yres < SCREEN_H ? v.yres : SCREEN_H;
	stride = f.line_length;
	offset = (off_t)v.yoffset * stride;
	if (!(buf = calloc(yres, stride)))
		return -1;
	return 0;
}

/* Copy screen rows [y0, y1) to fb0. */
static void put_rows(int y0, int y1)
{
	int y, x;

	for (y = y0; y < y1; y++) {
		if (FB_INVERT)
			for (x = 0; x < xres; x++)
				buf[y * stride + x] = 255 - screen[y * SCREEN_W + x];
		else
			memcpy(buf + y * stride, screen + y * SCREEN_W, xres);
	}
	if (y1 > y0 && pwrite(fd, buf + y0 * stride, (size_t)(y1 - y0) * stride,
	                      offset + (off_t)y0 * stride) != (ssize_t)(y1 - y0) * stride)
		logmsg("fb0 write: %s", strerror(errno));
}

void disp_flush(int full)
{
	if (fd < 0)
		return;
	put_rows(0, yres);
	if (ioctl(fd, FBIO_EINK_UPDATE_DISPLAY, full ? FX_UPDATE_FULL : FX_UPDATE_PARTIAL))
		logmsg("eink: update failed: %s", strerror(errno));
}

/* update_area_t from Lab126's linux/einkfb.h; x2/y2 are exclusive. */
struct update_area { int x1, y1, x2, y2, which_fx; unsigned char *buffer; };

void disp_area(int x, int y, int w, int h)
{
	struct update_area a;

	if (fd < 0)
		return;
	a.x1 = x < 0 ? 0 : x;
	a.y1 = y < 0 ? 0 : y;
	a.x2 = x + w > xres ? xres : x + w;
	a.y2 = y + h > yres ? yres : y + h;
	if (a.x2 <= a.x1 || a.y2 <= a.y1)
		return;
	a.which_fx = FX_UPDATE_FAST;
	a.buffer = NULL;                       /* take the pixels from fb0 */
	put_rows(a.y1, a.y2);
	if (ioctl(fd, FBIO_EINK_UPDATE_DISPLAY_AREA, &a))
		logmsg("eink: area update failed: %s", strerror(errno));
}
