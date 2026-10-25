/* Key input: every /dev/input/event* device, raw evdev key codes. */
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include "minios.h"

#define MAX_DEV 8

static struct pollfd fds[MAX_DEV];
static int nfds;

int input_init(void)
{
	DIR *d = opendir(INPUT_DIR);
	struct dirent *e;
	char path[300];

	if (!d)
		return -1;
	while ((e = readdir(d)) && nfds < MAX_DEV) {
		if (strncmp(e->d_name, "event", 5))
			continue;
		snprintf(path, sizeof path, "%s/%s", INPUT_DIR, e->d_name);
		if ((fds[nfds].fd = open(path, O_RDONLY | O_NONBLOCK)) >= 0) {
			fds[nfds++].events = POLLIN;
			logmsg("input: %s", path);
		}
	}
	closedir(d);
	return nfds ? 0 : -1;
}

/* Next key press (or auto-repeat) code, -1 on timeout. */
int input_raw(int timeout_ms)
{
	struct input_event ev;
	int i;

	if (poll(fds, nfds, timeout_ms) <= 0)
		return -1;
	for (i = 0; i < nfds; i++)
		if (fds[i].revents & POLLIN)
			while (read(fds[i].fd, &ev, sizeof ev) == sizeof ev)
				if (ev.type == EV_KEY && ev.value != 0)
					return ev.code;
	return -1;
}
