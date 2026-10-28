/* Drawing into the 8-bit gray screen buffer. Text goes through MuPDF's
 * built-in base-14 font so no font files are needed on the device. */
#include <string.h>
#include "minios.h"

static fz_font *font, *bold, *sym;

void gfx_init(void)
{
	const unsigned char *data;
	int len;

	font = fz_new_base14_font(ctx, FONT_NAME);
	bold = fz_new_base14_font(ctx, FONT_NAME "-Bold");
	if ((data = fz_lookup_noto_symbol2_font(ctx, &len)))   /* chess pieces */
		sym = fz_new_font_from_memory(ctx, "NotoSansSymbols2", data, len, 0, 0);
	else
		logmsg("no symbol font: chess pieces are letters");
}

void gfx_fill(int x, int y, int w, int h, int gray)
{
	if (x < 0) w += x, x = 0;
	if (y < 0) h += y, y = 0;
	if (x + w > SCREEN_W) w = SCREEN_W - x;
	if (y + h > SCREEN_H) h = SCREEN_H - y;
	for (; h > 0; h--, y++)
		if (w > 0)
			memset(screen + y * SCREEN_W + x, gray, w);
}

int gfx_text_w(const char *s, int px)
{
	float w = 0;
	int c;

	while (*s) {
		s += fz_chartorune(&c, s);
		w += fz_advance_glyph(ctx, font, fz_encode_character(ctx, font, c), 0);
	}
	return (int)(w * px + 0.5f);
}

static void draw_text(fz_font *font, int x, int baseline, int px, int gray, const char *s)
{
	float col = gray / 255.0f;
	fz_pixmap *pm = fz_new_pixmap_with_data(ctx, fz_device_gray(ctx), SCREEN_W, SCREEN_H,
	                                        NULL, 0, SCREEN_W, screen);
	fz_device *dev = fz_new_draw_device(ctx, fz_identity, pm);
	fz_text *t = fz_new_text(ctx);

	/* glyphs are y-up, the screen is y-down */
	fz_show_string(ctx, t, font, fz_make_matrix(px, 0, 0, -px, x, baseline), s,
	               0, 0, FZ_BIDI_LTR, FZ_LANG_UNSET);
	fz_fill_text(ctx, dev, t, fz_identity, fz_device_gray(ctx), &col, 1, fz_default_color_params);
	fz_close_device(ctx, dev);
	fz_drop_device(ctx, dev);
	fz_drop_text(ctx, t);
	fz_drop_pixmap(ctx, pm);
}

void gfx_text(int x, int baseline, int px, int gray, const char *s)
{
	draw_text(font, x, baseline, px, gray, s);
}

void gfx_text_bold(int x, int baseline, int px, int gray, const char *s)
{
	draw_text(bold, x, baseline, px, gray, s);
}

/* One symbol-font glyph centred on (cx, cy); -1 if there is no symbol font. */
int gfx_symbol(int cx, int cy, int px, int gray, int rune)
{
	char s[8];
	fz_rect r;

	if (!sym)
		return -1;
	r = fz_bound_glyph(ctx, sym, fz_encode_character(ctx, sym, rune), fz_make_matrix(px, 0, 0, -px, 0, 0));
	s[fz_runetochar(s, rune)] = 0;
	draw_text(sym, cx - (int)((r.x0 + r.x1) / 2), cy - (int)((r.y0 + r.y1) / 2), px, gray, s);
	return 0;
}

/* Draw s, cutting it down to "..." if wider than maxw. */
void gfx_text_fit(int x, int baseline, int px, int gray, const char *s, int maxw)
{
	char buf[256];
	size_t n = strlen(s);

	if (n > sizeof buf - 4)
		n = sizeof buf - 4;
	else if (gfx_text_w(s, px) <= maxw) {
		gfx_text(x, baseline, px, gray, s);
		return;
	}
	for (;;) {
		while (n > 0 && (s[n] & 0xC0) == 0x80)   /* stay on a UTF-8 boundary */
			n--;
		memcpy(buf, s, n);
		strcpy(buf + n, "...");
		if (n == 0 || gfx_text_w(buf, px) <= maxw)
			break;
		n--;
	}
	gfx_text(x, baseline, px, gray, buf);
}

/* Copy a gray image to (x, y), clipped to the screen. */
void gfx_blit(const uint8_t *src, int w, int h, int stride, int x, int y)
{
	int r, x0 = x < 0 ? -x : 0, x1 = x + w > SCREEN_W ? SCREEN_W - x : w;

	if (x1 <= x0)
		return;
	for (r = 0; r < h; r++)
		if (y + r >= 0 && y + r < SCREEN_H)
			memcpy(screen + (y + r) * SCREEN_W + x + x0, src + r * stride + x0, x1 - x0);
}

/* Full-screen one-line notice (USB, errors). */
void gfx_message(const char *s)
{
	int px = HEADER_PX * 2 / 3, x = (SCREEN_W - gfx_text_w(s, px)) / 2;

	gfx_fill(0, 0, SCREEN_W, SCREEN_H, 255);
	if (x < GRID_MARGIN)
		x = GRID_MARGIN;
	gfx_text_fit(x, SCREEN_H / 2, px, 0, s, SCREEN_W - 2 * GRID_MARGIN);
	disp_flush(1);
}

/* Rectangle outline of thickness t, drawn inside (x, y, w, h). */
void gfx_frame(int x, int y, int w, int h, int t, int gray)
{
	gfx_fill(x, y, w, t, gray);
	gfx_fill(x, y + h - t, w, t, gray);
	gfx_fill(x, y, t, h, gray);
	gfx_fill(x + w - t, y, t, h, gray);
}

void gfx_disc(int cx, int cy, int r, int gray)
{
	int dy, dx;

	for (dy = -r; dy <= r; dy++) {
		for (dx = 0; dx * dx + dy * dy <= r * r; dx++)
			;
		gfx_fill(cx - dx + 1, cy + dy, 2 * dx - 1, 1, gray);
	}
}

/* One black pixel in each 2x2 block: reads as light gray, yet stays
 * exact under the 1-bit fast refresh. */
void gfx_dither(int x, int y, int w, int h)
{
	int i, j;

	gfx_fill(x, y, w, h, 255);
	for (j = y & ~1; j < y + h; j += 2)
		for (i = x & ~1; i < x + w; i += 2)
			if (j >= y && i >= x && i < SCREEN_W && j < SCREEN_H)
				screen[j * SCREEN_W + i] = 0;
}

int key_wait(void)
{
	int k;

	while ((k = input_next(POLL_MS)) == KNONE)
		if (usb_online())
			return KNONE;
	return k;
}

int menu_run(const char *title, const char **items, int n, int sel, int *key)
{
	int h = (n + 1) * MENU_ROW + 2 * MENU_BORDER, x = (SCREEN_W - MENU_W) / 2,
	    y = (SCREEN_H - h) / 2, i, ry, first = 1;

	for (;;) {
		gfx_fill(x, y, MENU_W, h, 0);
		gfx_fill(x + MENU_BORDER, y + MENU_BORDER, MENU_W - 2 * MENU_BORDER, h - 2 * MENU_BORDER, 255);
		gfx_text_bold(x + (MENU_W - gfx_text_w(title, MENU_PX)) / 2,
		              y + MENU_BORDER + (MENU_ROW + MENU_PX) / 2 - 4, MENU_PX, 0, title);
		for (i = 0; i < n; i++) {
			ry = y + MENU_BORDER + (i + 1) * MENU_ROW;
			if (i == sel)
				gfx_fill(x + MENU_BORDER, ry, MENU_W - 2 * MENU_BORDER, MENU_ROW, 0);
			gfx_text_fit(x + MENU_BORDER + 20, ry + (MENU_ROW + MENU_PX) / 2 - 4, MENU_PX,
			             i == sel ? 255 : 0, items[i], MENU_W - 2 * MENU_BORDER - 40);
		}
		if (first)                         /* gray covers under the box: the 1-bit fast */
			disp_flush(0), first = 0;      /* waveform would leave them ghosting through */
		else
			disp_area(x, y, MENU_W, h);
		switch (*key = key_wait()) {
		case KUP:   sel = (sel + n - 1) % n; break;
		case KDOWN: sel = (sel + 1) % n; break;
		case KOK:   return sel;
		case KLEFT: case KRIGHT: case KNEXT: case KPREV: break;
		default:    return -1;               /* Back, Menu, Home, Power, USB */
		}
	}
}
