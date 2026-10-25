/* PC backend for `make host`: the screen goes to screen.pgm after every
 * refresh, keys come from stdin, and the file usb_plugged fakes the cable.
 *   w/a/s/d = 5-way   e = select   n/p = page   h = home   m = menu   b = back   q = power
 * End of input exits, so a session can be scripted: printf 'dden' | ... */
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "minios.h"

int disp_init(void) { return 0; }

void disp_flush(int full)
{
	FILE *f = fopen("screen.pgm.tmp", "wb");

	if (!f)
		return;
	fprintf(f, "P5\n%d %d\n255\n", SCREEN_W, SCREEN_H);
	fwrite(screen, 1, sizeof screen, f);
	fclose(f);
	rename("screen.pgm.tmp", "screen.pgm");
	printf("[refresh %s]\n", full ? "full" : "partial");
	fflush(stdout);
}

void disp_area(int x, int y, int w, int h) { disp_flush(0); }

int input_init(void) { return 0; }

int input_raw(int timeout_ms)
{
	struct pollfd p = { 0, POLLIN, 0 };
	char c;

	if (poll(&p, 1, timeout_ms) <= 0)
		return -1;
	if (read(0, &c, 1) != 1)
		exit(0);
	switch (c) {
	case 'w': return K_UP;
	case 's': return K_DOWN;
	case 'a': return K_LEFT;
	case 'd': return K_RIGHT;
	case 'e': return K_SELECT;
	case 'n': return K_NEXT;
	case 'p': return K_PREV;
	case 'h': return K_HOME;
	case 'm': return K_MENU;
	case 'b': return K_BACK;
	case 'q': return K_POWER;
	}
	return -1;
}

int usb_online(void) { return access("usb_plugged", F_OK) == 0; }
void usb_export(void) { puts("[usb export]"); }
void usb_unexport(void) { puts("[usb unexport]"); }
