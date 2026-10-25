/* minios: PID 1 on the Kindle. Mounts the basics, then runs the UI in a
 * child process and respawns it if it ever dies.
 *   minios        normal start
 *   minios keys   print raw key codes (also when .minios/keys exists) */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "minios.h"

#define PER_PAGE (GRID_COLS * GRID_ROWS)
#define LOG_FILE STATE_DIR "/log.txt"

fz_context *ctx;
uint8_t screen[SCREEN_W * SCREEN_H];
static struct book books[MAX_BOOKS];

void logmsg(const char *fmt, ...)
{
	struct stat st;
	va_list ap;
	FILE *f;

	if (!stat(LOG_FILE, &st) && st.st_size > 256 * 1024)   /* keep the log small */
		remove(LOG_FILE);
	if (!(f = fopen(LOG_FILE, "a")))
		return;
	fprintf(f, "%ld ", (long)time(NULL));
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}

static long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000;
}

long saver_ms;
static long last_key;                          /* last key press, or end of USB mode */

/* Fit an image from the drive root onto the cleared screen. */
static int saver_image(const char *name)
{
	char path[128];
	fz_document *doc = NULL;
	fz_page *page = NULL;
	fz_pixmap *pm = NULL;
	fz_rect r;
	float s;
	int ok = 0;

	snprintf(path, sizeof path, USERSTORE_MNT "/%s", name);
	if (access(path, R_OK))
		return 0;
	fz_var(doc);
	fz_var(page);
	fz_var(pm);
	fz_var(ok);
	fz_try(ctx) {
		doc = fz_open_document(ctx, path);
		page = fz_load_page(ctx, doc, 0);
		r = fz_bound_page(ctx, page);
		s = SCREEN_W / (r.x1 - r.x0);
		if (s * (r.y1 - r.y0) > SCREEN_H)
			s = SCREEN_H / (r.y1 - r.y0);
		pm = fz_new_pixmap_from_page(ctx, page, fz_scale(s, s), fz_device_gray(ctx), 0);
		gfx_blit(fz_pixmap_samples(ctx, pm), fz_pixmap_width(ctx, pm), fz_pixmap_height(ctx, pm),
		         fz_pixmap_stride(ctx, pm), (SCREEN_W - fz_pixmap_width(ctx, pm)) / 2,
		         (SCREEN_H - fz_pixmap_height(ctx, pm)) / 2);
		ok = 1;
	}
	fz_always(ctx) {
		fz_drop_pixmap(ctx, pm);
		fz_drop_page(ctx, page);
		fz_drop_document(ctx, doc);
	}
	fz_catch(ctx)
		logmsg("screensaver %s: %s", name, fz_caught_message(ctx));
	return ok;
}

/* Show the screensaver until a key (swallowed) or the USB cable, then put
 * back exactly what was on screen. */
static void screensaver(void)
{
	static uint8_t saved[SCREEN_W * SCREEN_H];
	static const char *files[] = SAVER_FILES;
	long t0 = now_ms();
	int i, shown = 0, c;

	memcpy(saved, screen, sizeof saved);
	gfx_fill(0, 0, SCREEN_W, SCREEN_H, 255);
	for (i = 0; i < (int)(sizeof files / sizeof *files) && !shown; i++)
		shown = saver_image(files[i]);
	if (!shown) {
		gfx_text_bold((SCREEN_W - gfx_text_w("minios", 72)) / 2, SCREEN_H / 2, 72, 0, "minios");
		gfx_text((SCREEN_W - gfx_text_w("Press any key", 22)) / 2, SCREEN_H / 2 + 60, 22, 0,
		         "Press any key");
	}
	disp_flush(1);
	while ((c = input_raw(POLL_MS)) < 0 && !usb_online())
		;
	memcpy(screen, saved, sizeof saved);
	if (c >= 0)
		disp_flush(1);                     /* USB: its own screen comes next */
	saver_ms += now_ms() - t0;
}

int input_next(int timeout_ms)
{
	static long last;
	static int last_code = -1;
	int c = input_raw(timeout_ms);
	long t = now_ms();

	if (!last_key)
		last_key = t;
	if (c < 0 && t - last_key >= SAVER_IDLE_MS) {
		screensaver();
		last_key = now_ms();
	}
	if (c >= 0)
		last_key = t;
	if (c < 0 || (c == last_code && t - last < KEY_REPEAT_MS))
		return KNONE;
	last = t;
	last_code = c;
	switch (c) {
	case K_UP:     return KUP;
	case K_DOWN:   return KDOWN;
	case K_LEFT:   return KLEFT;
	case K_RIGHT:  return KRIGHT;
	case K_SELECT: return KOK;
	case K_NEXT:
	case K_NEXT2:  return KNEXT;
	case K_PREV:
	case K_PREV2:  return KPREV;
	case K_HOME:   return KHOME;
	case K_BACK:   return KBACK;
	case K_MENU:   return KMENU;
	case K_POWER:  return KPOWER;
	}
	return KNONE;
}

static void make_dirs(void)
{
	mkdir(BOOKS_DIR, 0755);
	mkdir(STATE_DIR, 0755);
	mkdir(COVER_DIR, 0755);
}

static void sys_init(void)
{
	setenv("PATH", "/sbin:/bin:/usr/sbin:/usr/bin", 1);
	mount("/", "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);   /* all state is on the userstore */
	mount("proc", "/proc", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	mount("tmpfs", "/dev", "tmpfs", 0, "mode=0755");
	mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
	system("mdev -s");                     /* device nodes, see /etc/mdev.conf */
	usb_unexport();                        /* attach + mount the userstore */
	system("/etc/rc.local");               /* modules + install/restore triggers */
	system("mdev -s");                     /* nodes of the modules just loaded (keys) */
}

static void power_off(void)
{
	gfx_message("minios is off. Press power to start.");
#ifdef HOST
	exit(0);
#else
	sync();
	umount(USERSTORE_MNT);
	reboot(RB_POWER_OFF);
#endif
}

static void usb_mode(void)
{
	gfx_message("USB connected - copy EPUB or PDF books onto the Kindle drive.");
	usb_export();
	while (usb_online())
		input_raw(POLL_MS);                /* drain keys while the PC owns the files */
	usb_unexport();
	make_dirs();
	system("/etc/rc.local");               /* apply DO_INSTALL / DO_RESTORE copied by the PC */
	last_key = now_ms();                   /* the idle minute starts now */
}

/* The battery box: charge and how long the battery lasts at the current drain. */
static int battery_box(void)
{
	int pct, mah, ma, m, key;
	char title[32], line[48];
	const char *items[] = { line };

	if (battery_read(&pct, &mah, &ma))
		return KNONE;
	logmsg("battery %d%% %d mAh %d mA", pct, mah, ma);
	snprintf(title, sizeof title, "Battery %d%%", pct);
	if (ma > 0)
		snprintf(line, sizeof line, "Charging");
	else if (ma == 0)
		snprintf(line, sizeof line, "Estimating, try later");
	else {
		m = mah * 60 / -ma;
		snprintf(line, sizeof line, "About %d h %d min left", m / 60, m % 60);
	}
	menu_run(title, items, 1, 0, &key);
	return key;
}

static int show_keys(void)
{
	char s[64];
	int c;

	for (;;) {
		if ((c = input_raw(-1)) < 0)
			continue;
		snprintf(s, sizeof s, "key code %d", c);
		printf("%s\n", s);
		fflush(stdout);
		logmsg("%s", s);
		gfx_message(s);
	}
}

static int ui(int keys)
{
	int n, sel = 0, key, page, full = 1, on_batt = 0, pct, mah, ma;

	if (!(ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT)))
		return 1;
	fz_register_document_handlers(ctx);
	make_dirs();
	logmsg("minios start");
	if (disp_init() || input_init())
		logmsg("display or input init failed");
	gfx_init();
	if (keys || access(STATE_DIR "/keys", F_OK) == 0)
		return show_keys();
	if (!battery_read(&pct, &mah, &ma))
		logmsg("battery %d%% %d mAh %d mA", pct, mah, ma);

	n = lib_scan(books, MAX_BOOKS);
	for (;;) {
		if (usb_online()) {
			usb_mode();
			n = lib_scan(books, MAX_BOOKS);
			sel = 0;
			full = 1;
		}
		lib_draw(books, n, sel, on_batt);
		disp_flush(full);
		full = 0;
		while ((key = input_next(POLL_MS)) == KNONE && !usb_online())
			;
		page = sel / PER_PAGE;
		switch (key) {
		case KLEFT:  if (sel > 0 && !on_batt) sel--; break;
		case KRIGHT: if (sel < n - 1 && !on_batt) sel++; break;
		case KUP:                              /* top row: up to the battery */
			if (sel % PER_PAGE >= GRID_COLS)
				sel -= GRID_COLS;
			else if (!battery_read(&pct, &mah, &ma))
				on_batt = 1;
			else if (sel >= GRID_COLS)
				sel -= GRID_COLS;
			break;
		case KDOWN:
			if (on_batt)
				on_batt = 0;
			else if (sel + GRID_COLS < n)
				sel += GRID_COLS;
			break;
		case KNEXT:  if ((page + 1) * PER_PAGE < n) sel = (page + 1) * PER_PAGE; break;
		case KPREV:  if (page > 0) sel = (page - 1) * PER_PAGE; break;
		case KPOWER: power_off(); full = 1; break;
		case KOK:
			if (on_batt) {
				if (battery_box() == KPOWER)
					power_off(), full = 1;
				break;
			}
			if (n && reader_run(books[sel].name) == KPOWER)
				power_off();
			full = 1;
			break;
		case KMENU: {
			static const char *games[] = { "Sudoku", "Checkers" };
			int g = menu_run("Games", games, 2, 0, &key);

			if (g >= 0)
				key = games_run(g == 0 ? SUDOKU : CHECKERS);
			if (key == KPOWER)
				power_off();
			full = g >= 0 || key == KPOWER;    /* a closed menu only needs a redraw */
			break;
		}
		}
		if (sel / PER_PAGE != page)
			full = 1;
	}
}

int main(int argc, char **argv)
{
	if (getpid() == 1) {
		sys_init();
		for (;;) {                         /* respawn the UI; reap every child */
			pid_t pid = fork();
			if (pid == 0)
				break;
			while (pid > 0 && wait(NULL) != pid)
				;
			sleep(1);
		}
	}
	return ui(argc > 1 && !strcmp(argv[1], "keys"));
}
