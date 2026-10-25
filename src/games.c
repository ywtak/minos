/* Sudoku and Checkers screens. Everything drawn here is pure black/white,
 * so cursor moves and single changes use the fast 1-bit refresh of just the
 * changed squares; opening, a new game and the end use a full refresh.
 * Each screen keeps the last drawn look of every square and redraws only
 * the squares whose look changed. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "minios.h"
#include "games.h"

static int quit;                               /* KHOME, KPOWER, KNONE (USB); -1 = playing */
static char buf[1024];

static int load_file(const char *path)
{
	FILE *f = fopen(path, "r");
	size_t n;

	if (!f)
		return -1;
	n = fread(buf, 1, sizeof buf - 1, f);
	fclose(f);
	buf[n] = 0;
	return 0;
}

static int game_key(void)
{
	int k = key_wait();

	if (k == KHOME || k == KPOWER || k == KNONE)
		quit = k;
	return k;
}

static int game_menu(const char *title, const char **items, int n, int sel)
{
	int k, r = menu_run(title, items, n, sel, &k);

	if (r < 0 && (k == KHOME || k == KPOWER || k == KNONE))
		quit = k;
	return r;
}

static void center_text(int y, int px, int bold, const char *s)
{
	int x = (SCREEN_W - gfx_text_w(s, px)) / 2;

	if (bold)
		gfx_text_bold(x, y, px, 0, s);
	else
		gfx_text(x, y, px, 0, s);
}

/* One centred line of text (title or status) at baseline y. */
static void line(int y, const char *s, int refresh)
{
	gfx_fill(0, y - GAME_STATUS_PX - 4, SCREEN_W, GAME_STATUS_PX + 12, 255);
	center_text(y, GAME_STATUS_PX, 0, s);
	if (refresh)
		disp_area(0, y - GAME_STATUS_PX - 4, SCREEN_W, GAME_STATUS_PX + 12);
}

/* Quiet end screen. 1 = Select (new game), 0 = leave. */
static int end_screen(const char *title, const char *sub)
{
	int k;

	gfx_fill(0, 0, SCREEN_W, SCREEN_H, 255);
	center_text(SCREEN_H / 2 - END_PX, END_PX, 1, title);
	if (sub)
		center_text(SCREEN_H / 2 + GAME_STATUS_PX, GAME_STATUS_PX, 0, sub);
	center_text(SCREEN_H - 3 * GAME_STATUS_PX, GAME_STATUS_PX, 0, "Select: new game     Back: books");
	disp_flush(1);
	while ((k = game_key()) != KOK && quit < 0)
		if (k == KBACK)
			quit = KHOME;
	return quit < 0;
}

/* grow the box (x0,y0)-(x1,y1) to cover a square */
static void grow(int *box, int x, int y, int w)
{
	if (x < box[0]) box[0] = x;
	if (y < box[1]) box[1] = y;
	if (x + w > box[2]) box[2] = x + w;
	if (y + w > box[3]) box[3] = y + w;
}

static void flush_box(int *box)
{
	if (box[2] > box[0])
		disp_area(box[0], box[1], box[2] - box[0], box[3] - box[1]);
}

/* ---- Sudoku ------------------------------------------------------------ */

#define SX     ((SCREEN_W - 9 * SDK_CELL) / 2)
#define SY     ((SCREEN_H - 9 * SDK_CELL) / 2)
#define PICK_Y (SY + 9 * SDK_CELL + SDK_CELL / 3)
#define PICK_W (9 * SDK_CELL / 10)

static const char *sdk_levels[] = { "Easy", "Medium", "Hard" };
static struct sudoku sdk;
static uint8_t sdk_looks[81];
static int cur, sdk_base;                      /* cursor; seconds before this sitting */
static long sdk_t0, sdk_saver0;               /* start of this sitting; saver_ms then */
static char sdk_msg[64];

static int sdk_secs(void) { return sdk_base + (mono_ms() - sdk_t0 - (saver_ms - sdk_saver0)) / 1000; }

static void sdk_store(void)
{
	sdk.secs = sdk_secs();
	sdk_save(&sdk, buf, sizeof buf);
	if (file_put(SUDOKU_SAVE, buf))
		logmsg("sudoku: could not save");
}

static int sdk_look(int i)
{
	return sdk.grid[i] | sdk.given[i] << 4 | (sdk.show_conflicts && sdk_conflict(&sdk, i)) << 5 |
	       sdk.wrong[i] << 6 | (i == cur) << 7;
}

static void sdk_cell(int i, int l)
{
	int x = SX + i % 9 * SDK_CELL + 2, y = SY + i / 9 * SDK_CELL + 2, w = SDK_CELL - 4;
	int base = y + w / 2 + SDK_PX * 36 / 100, mark = SDK_PX * 2 / 3;
	char t[2] = { '0' + (l & 15), 0 };

	gfx_fill(x, y, w, w, 255);
	if (l & 0x80)
		gfx_frame(x + 1, y + 1, w - 2, w - 2, CURSOR_W, 0);
	if (!(l & 15))
		return;
	if (l & 0x10)
		gfx_text_bold(x + (w - gfx_text_w(t, SDK_PX)) / 2, base, SDK_PX, 0, t);
	else
		gfx_text(x + (w - gfx_text_w(t, SDK_PX)) / 2, base, SDK_PX, 0, t);
	if (l & 0x20)                              /* conflict: underline */
		gfx_fill(x + (w - mark) / 2, base + 5, mark, 2, 0);
	if (l & 0x40)                              /* wrong after Check: struck through */
		gfx_fill(x + (w - mark) / 2 - 2, base - SDK_PX * 36 / 100, mark + 4, 2, 0);
}

static void sdk_title(int refresh)
{
	char t[64];

	snprintf(t, sizeof t, "Sudoku - %s", sdk_levels[sdk.level]);
	line(SY - SDK_CELL / 2, sdk_msg[0] ? sdk_msg : t, refresh);
}

static void sdk_draw(void)
{
	int k, t, i;

	gfx_fill(0, 0, SCREEN_W, SCREEN_H, 255);
	sdk_title(0);
	for (k = 0; k <= 9; k++) {
		t = k % 3 ? SDK_THIN : SDK_THICK;
		gfx_fill(SX + k * SDK_CELL - t / 2, SY - 1, t, 9 * SDK_CELL + 2, 0);
		gfx_fill(SX - 1, SY + k * SDK_CELL - t / 2, 9 * SDK_CELL + 2, t, 0);
	}
	for (i = 0; i < 81; i++)
		sdk_cell(i, sdk_looks[i] = sdk_look(i));
}

static void sdk_refresh(void)
{
	int i, l, box[4] = { SCREEN_W, SCREEN_H, 0, 0 };

	for (i = 0; i < 81; i++)
		if ((l = sdk_look(i)) != sdk_looks[i]) {
			sdk_cell(i, sdk_looks[i] = l);
			grow(box, SX + i % 9 * SDK_CELL, SY + i / 9 * SDK_CELL, SDK_CELL);
		}
	flush_box(box);
}

/* The 1-9 / Erase row. Returns the digit, 0 to erase, -1 to cancel. */
static int sdk_picker(void)
{
	int sel = sdk.grid[cur] ? sdk.grid[cur] - 1 : 0, i, k, x, px;
	char t[2] = { 0, 0 };
	const char *label;

	for (;;) {
		gfx_fill(SX - 2, PICK_Y, 9 * SDK_CELL + 4, SDK_CELL, 255);
		for (i = 0; i < 10; i++) {
			x = SX + i * PICK_W;
			t[0] = '1' + i;
			label = i < 9 ? t : "Erase";
			px = i < 9 ? SDK_PX * 3 / 4 : GAME_STATUS_PX * 3 / 4;
			gfx_frame(x, PICK_Y, PICK_W + 1, SDK_CELL, SDK_THIN, 0);
			if (i == sel)
				gfx_frame(x + 2, PICK_Y + 2, PICK_W - 3, SDK_CELL - 4, CURSOR_W, 0);
			gfx_text(x + (PICK_W - gfx_text_w(label, px)) / 2, PICK_Y + (SDK_CELL + px) / 2 - 2, px, 0, label);
		}
		disp_area(SX - 2, PICK_Y, 9 * SDK_CELL + 4, SDK_CELL);
		k = game_key();
		if (k == KLEFT)
			sel = (sel + 9) % 10;
		else if (k == KRIGHT)
			sel = (sel + 1) % 10;
		else if (k == KOK || k == KBACK || quit >= 0)
			break;
	}
	gfx_fill(SX - 2, PICK_Y, 9 * SDK_CELL + 4, SDK_CELL, 255);
	disp_area(SX - 2, PICK_Y, 9 * SDK_CELL + 4, SDK_CELL);
	return k == KOK && quit < 0 ? (sel < 9 ? sel + 1 : 0) : -1;
}

/* Ask the difficulty and make a puzzle. 0 if the menu was closed. */
static int sdk_new_game(void)
{
	int lv = game_menu("New game", sdk_levels, 3, sdk.level), clues;
	long t;

	if (lv < 0)
		return 0;
	t = mono_ms();
	clues = sdk_new(&sdk, lv);
	logmsg("sudoku %s: %d clues in %ld ms", sdk_levels[lv], clues, mono_ms() - t);
	sdk_base = 0;
	sdk_t0 = mono_ms(), sdk_saver0 = saver_ms;
	sdk_msg[0] = 0;
	cur = 40;
	sdk_store();
	return 1;
}

static int sudoku_run(void)
{
	char items[4][32], took[32];
	const char *menu[4] = { items[0], items[1], items[2], items[3] };
	int k, i, d, n;

	if (load_file(SUDOKU_SAVE) || sdk_load(&sdk, buf)) {
		memset(&sdk, 0, sizeof sdk);           /* no save, or a damaged one */
		sdk.level = 1;
		sdk.show_conflicts = 1;
		if (!sdk_new_game())
			return quit < 0 ? KHOME : quit;
	} else {
		sdk_base = sdk.secs;
		sdk_t0 = mono_ms(), sdk_saver0 = saver_ms;
		cur = 40;
	}
	sdk_msg[0] = 0;
	sdk_draw();
	disp_flush(1);
	while (quit < 0) {
		k = game_key();
		if (sdk_msg[0] && quit < 0) {          /* a message lasts one key */
			sdk_msg[0] = 0;
			sdk_title(1);
		}
		switch (k) {
		case KUP:    if (cur >= 9) cur -= 9; break;
		case KDOWN:  if (cur < 72) cur += 9; break;
		case KLEFT:  if (cur % 9) cur--; break;
		case KRIGHT: if (cur % 9 < 8) cur++; break;
		case KBACK:  quit = KHOME; break;
		case KOK:
			if (sdk.given[cur] || (d = sdk_picker()) < 0)
				break;
			sdk.grid[cur] = d;
			sdk.wrong[cur] = 0;
			if (!sdk_solved(&sdk)) {
				sdk_store();
				break;
			}
			n = sdk_secs();
			remove(SUDOKU_SAVE);
			snprintf(took, sizeof took, "in %d:%02d", n / 60, n % 60);
			if (!end_screen("Solved", took) || !sdk_new_game()) {
				if (quit < 0)
					quit = KHOME;
				break;
			}
			sdk_draw();
			disp_flush(1);
			continue;
		case KMENU:
			strcpy(items[0], "New game");
			strcpy(items[1], "Check");
			strcpy(items[2], sdk.show_conflicts ? "Show conflicts: on" : "Show conflicts: off");
			strcpy(items[3], "Quit");
			switch (game_menu("Sudoku", menu, 4, 0)) {
			case 0:
				if (sdk_new_game()) {
					sdk_draw();
					disp_flush(1);
					continue;
				}
				break;
			case 1:
				for (i = n = 0; i < 81; i++)
					if (sdk.grid[i] && !sdk.given[i] && sdk.grid[i] != sdk.sol[i])
						sdk.wrong[i] = 1, n++;
				if (n)
					snprintf(sdk_msg, sizeof sdk_msg, "%d wrong", n);
				else
					strcpy(sdk_msg, "No mistakes so far");
				sdk_store();
				break;
			case 2:
				sdk.show_conflicts = !sdk.show_conflicts;
				sdk_store();
				break;
			case 3:
				quit = KHOME;
				break;
			}
			if (quit < 0) {                    /* repaint what the menu covered */
				sdk_draw();
				disp_area(0, 0, SCREEN_W, SCREEN_H);
			}
			continue;
		}
		if (quit < 0)
			sdk_refresh();
	}
	if (!access(SUDOKU_SAVE, F_OK))            /* not after a solved puzzle */
		sdk_store();
	return quit;
}

/* ---- Checkers ---------------------------------------------------------- */

#define BX       ((SCREEN_W - 8 * CK_SQ) / 2)
#define BY       ((SCREEN_H - 8 * CK_SQ) / 2)
#define STATUS_Y (BY + 8 * CK_SQ + CK_SQ * 2 / 3)

static const char *ck_levels[] = { "Easy", "Normal", "Hard" };
static const char *ck_modes[] = { "vs Kindle", "2 players" };
static const char *ck_saves[] = CHECKERS_SAVE;
static struct ck ck, undo[CK_UNDO];
static int ck_mode, ck_level = 1, csel, ccur, nundo;
static uint8_t ck_looks[64];
static char ck_status[64];

static void ck_store(void)
{
	ck_save(&ck, ck_level, buf, sizeof buf);
	if (file_put(ck_saves[ck_mode], buf))
		logmsg("checkers: could not save");
}

/* Load the current mode's game, or start one. */
static void ck_open(void)
{
	int lv;

	if (load_file(ck_saves[ck_mode]) || ck_load(&ck, &lv, buf))
		ck_new(&ck);
	else if (ck_mode == 0)
		ck_level = lv;
	csel = -1;
	nundo = 0;
}

static int ck_dot(int s)
{
	struct step st[CK_MAXSTEPS];
	int i, n;

	if (csel < 0)
		return 0;
	for (i = 0, n = ck_steps(&ck, st); i < n; i++)
		if (st[i].from == csel && st[i].to == s)
			return 1;
	return 0;
}

static int ck_look(int s)
{
	return (ck.b[s] + 2) | (s == ccur) << 3 | (s == csel) << 4 | ck_dot(s) << 5;
}

static void ck_square(int s, int l)
{
	int x = BX + s % 8 * CK_SQ, y = BY + s / 8 * CK_SQ, p = (l & 7) - 2;
	int cx = x + CK_SQ / 2, cy = y + CK_SQ / 2;

	if ((s / 8 + s % 8) % 2)
		gfx_dither(x, y, CK_SQ, CK_SQ);
	else
		gfx_fill(x, y, CK_SQ, CK_SQ, 255);
	if (p) {
		gfx_disc(cx, cy, CK_PIECE_R, 0);
		if (p > 0)
			gfx_disc(cx, cy, CK_PIECE_R - CK_OUTLINE, 255);
		if (abs(p) == 2) {                     /* king: an inner ring */
			gfx_disc(cx, cy, CK_KING_R, p > 0 ? 0 : 255);
			gfx_disc(cx, cy, CK_KING_R - CK_OUTLINE, p > 0 ? 255 : 0);
		}
	}
	if (l & 0x20)
		gfx_disc(cx, cy, CK_DOT_R, 0);
	if (l & 0x10)
		gfx_frame(x, y, CK_SQ, CK_SQ, CK_SEL_W, 0);
	if (l & 0x08)
		gfx_frame(x, y, CK_SQ, CK_SQ, CURSOR_W, 0);
}

static void ck_draw(void)
{
	char t[64];
	int s;

	gfx_fill(0, 0, SCREEN_W, SCREEN_H, 255);
	if (ck_mode == 0)
		snprintf(t, sizeof t, "Checkers - vs Kindle, %s", ck_levels[ck_level]);
	else
		snprintf(t, sizeof t, "Checkers - 2 players");
	line(BY - CK_SQ / 2, t, 0);
	gfx_frame(BX - 2, BY - 2, 8 * CK_SQ + 4, 8 * CK_SQ + 4, 2, 0);
	for (s = 0; s < 64; s++)
		ck_square(s, ck_looks[s] = ck_look(s));
	line(STATUS_Y, ck_status, 0);
}

static void ck_refresh(const char *status)
{
	int s, l, box[4] = { SCREEN_W, SCREEN_H, 0, 0 };

	for (s = 0; s < 64; s++)
		if ((l = ck_look(s)) != ck_looks[s]) {
			ck_square(s, ck_looks[s] = l);
			grow(box, BX + s % 8 * CK_SQ, BY + s / 8 * CK_SQ, CK_SQ);
		}
	flush_box(box);
	if (strcmp(status, ck_status)) {
		snprintf(ck_status, sizeof ck_status, "%s", status);
		line(STATUS_Y, ck_status, 1);
	}
}

static void ck_restart(void)
{
	ck_new(&ck);
	csel = -1;
	nundo = 0;
	ck_store();
}

static void ck_menu(void)
{
	char items[5][32];
	const char *menu[5] = { items[0], items[1], items[2], items[3], items[4] };
	int n = 0, r;
	FILE *f;

	strcpy(items[n++], "New game");
	snprintf(items[n++], 32, "Mode: %s", ck_modes[ck_mode]);
	if (ck_mode == 0) {
		snprintf(items[n++], 32, "Difficulty: %s", ck_levels[ck_level]);
		strcpy(items[n++], "Undo last move");
	}
	strcpy(items[n++], "Quit");
	r = game_menu("Checkers", menu, n, 0);
	if (r >= 0 && !strcmp(menu[r], "New game")) {
		ck_restart();
		ck_draw();
		disp_flush(1);
		return;
	} else if (r >= 0 && !strncmp(menu[r], "Mode", 4)) {
		r = game_menu("Mode", ck_modes, 2, ck_mode);
		if (r >= 0 && r != ck_mode) {
			ck_store();
			ck_mode = r;
			if ((f = fopen(CHECKERS_MODE, "w"))) {
				fprintf(f, "%d\n", ck_mode);
				fclose(f);
			}
			ck_open();
			ck_draw();
			disp_flush(1);
			return;
		}
	} else if (r >= 0 && !strncmp(menu[r], "Difficulty", 10)) {
		if ((r = game_menu("Difficulty", ck_levels, 3, ck_level)) >= 0) {
			ck_level = r;
			ck_store();
		}
	} else if (r >= 0 && !strcmp(menu[r], "Undo last move")) {
		if (nundo) {
			ck = undo[--nundo];
			csel = -1;
			ck_store();
		}
	} else if (r >= 0)
		quit = KHOME;
	if (quit < 0) {                            /* repaint what the menu covered */
		ck_draw();
		disp_area(0, 0, SCREEN_W, SCREEN_H);
	}
}

static int checkers_run(void)
{
	static const int depths[] = CK_DEPTH;
	struct step st[CK_MAXSTEPS], m;
	const char *status;
	int k, i, n, reached;
	long t;

	ck_mode = load_file(CHECKERS_MODE) || buf[0] != '1' ? 0 : 1;
	ck_open();
	ccur = 5 * 8;                              /* a white man, bottom left */
	ck_status[0] = 0;
	ck_draw();
	disp_flush(1);
	while (quit < 0) {
		if (!(n = ck_steps(&ck, st))) {        /* no move left: that side lost */
			const char *who = ck_mode == 0 ? (ck.turn < 0 ? "You win" : "Kindle wins")
			                               : (ck.turn < 0 ? "White wins" : "Black wins");
			ck_restart();                      /* leaving now opens a new game */
			if (!end_screen(who, NULL))
				break;
			ck_draw();
			disp_flush(1);
			continue;
		}
		if (ck_mode == 0 && ck.turn < 0) {     /* the Kindle plays black */
			ck_refresh("Kindle is thinking...");
			t = mono_ms();
			m = ck_ai(&ck, depths[ck_level], ck_level == 0 ? CK_EASY_RANDOM : 0, CK_AI_MS, &reached);
			logmsg("checkers: depth %d in %ld ms", reached, mono_ms() - t);
			ck_play(&ck, m);
			ck_store();
			continue;
		}
		if (ck.chain >= 0)
			status = "Jump again";
		else if (ck_mode == 0)
			status = "Your move";
		else
			status = ck.turn > 0 ? "White to move" : "Black to move";
		ck_refresh(status);
		k = game_key();
		switch (k) {
		case KUP:    if (ccur >= 8) ccur -= 8; break;
		case KDOWN:  if (ccur < 56) ccur += 8; break;
		case KLEFT:  if (ccur % 8) ccur--; break;
		case KRIGHT: if (ccur % 8 < 7) ccur++; break;
		case KMENU:  ck_menu(); break;
		case KBACK:
			if (csel >= 0 && ck.chain < 0)
				csel = -1;
			else
				quit = KHOME;
			break;
		case KOK:
			for (i = 0; i < n && !(st[i].from == csel && st[i].to == ccur); i++)
				;
			if (csel >= 0 && i < n) {          /* a dot: move there */
				if (ck.chain < 0 && ck_mode == 0) {
					if (nundo == CK_UNDO)
						memmove(undo, undo + 1, sizeof undo - sizeof *undo), nundo--;
					undo[nundo++] = ck;
				}
				csel = ck_play(&ck, st[i]) ? ck.chain : -1;
				ck_store();
				break;
			}
			for (i = 0; i < n && st[i].from != ccur; i++)
				;
			if (ck.chain < 0 && i < n)         /* pick up a piece that can move */
				csel = ccur;
			break;
		}
	}
	ck_store();
	return quit;
}

int games_run(enum game g)
{
	static int seeded;
	uint32_t s = 0;
	int fd;

	if (!seeded) {                             /* the clock restarts at every boot */
		if ((fd = open("/dev/urandom", O_RDONLY)) >= 0) {
			if (read(fd, &s, sizeof s) != sizeof s)
				s = 0;
			close(fd);
		}
		rng_seed(s ^ (uint32_t)mono_ms());
		seeded = 1;
	}
	quit = -1;
	return g == SUDOKU ? sudoku_run() : checkers_run();
}
