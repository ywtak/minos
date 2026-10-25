/* "All books": find every EPUB/PDF on the USB drive, cache one cover per
 * book, draw the grid. Book names are paths relative to USERSTORE_MNT. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "minios.h"

#define PER_PAGE (GRID_COLS * GRID_ROWS)
#define CELL_W ((SCREEN_W - 2 * GRID_MARGIN - (GRID_COLS - 1) * GRID_GAP) / GRID_COLS)
#define CELL_H ((SCREEN_H - HEADER_H - FOOTER_H - GRID_MARGIN - (GRID_ROWS - 1) * GRID_GAP) / GRID_ROWS)

static int is_book_file(const char *name)
{
	const char *e = strrchr(name, '.');
	return name[0] != '.' && e && (!strcasecmp(e, ".epub") || !strcasecmp(e, ".pdf"));
}

/* Cache name changes when the file changes, so stale covers are never shown. */
static void cover_path(const char *name, char *out, size_t n)
{
	char full[512];
	struct stat st = {0};
	unsigned h = 5381;
	const char *p;

	snprintf(full, sizeof full, "%s/%s", USERSTORE_MNT, name);
	stat(full, &st);
	for (p = name; *p; p++)
		h = h * 33 + (unsigned char)*p;
	snprintf(out, n, "%s/%08x-%lx-%lx.pgm", COVER_DIR, h, (long)st.st_size, (long)st.st_mtime);
}

/* Read a cached cover. px == NULL reads only the title. */
static int cover_read(const char *name, char *title, size_t tn, int *w, int *h, uint8_t **px)
{
	char path[512], line[256];
	FILE *f;
	int ok = 0;

	cover_path(name, path, sizeof path);
	if (!(f = fopen(path, "rb")))
		return 0;
	if (fgets(line, sizeof line, f) && !strncmp(line, "P5", 2) &&
	    fgets(line, sizeof line, f) && line[0] == '#' &&
	    fscanf(f, "%d %d 255", w, h) == 2 && fgetc(f) != EOF) {
		line[strcspn(line, "\n")] = 0;
		snprintf(title, tn, "%s", line + 2);
		ok = 1;
		if (px) {
			*px = malloc((size_t)*w * *h);
			ok = *px && fread(*px, 1, (size_t)*w * *h, f) == (size_t)*w * *h;
			if (!ok)
				free(*px);
		}
	}
	fclose(f);
	return ok;
}

/* Render the first page of the book into the cache, with its title. */
static void cover_make(const char *name)
{
	char full[512], path[512], title[160] = "";
	fz_document *doc = NULL;
	fz_page *page = NULL;
	fz_pixmap *pm = NULL;
	FILE *f = NULL;

	snprintf(full, sizeof full, "%s/%s", USERSTORE_MNT, name);
	cover_path(name, path, sizeof path);
	fz_var(doc);
	fz_var(page);
	fz_var(pm);
	fz_try(ctx) {
		fz_rect r;
		float s;
		int y;

		doc = fz_open_document(ctx, full);
		if (fz_is_document_reflowable(ctx, doc))
			fz_layout_document(ctx, doc, SCREEN_W, SCREEN_H, READER_EM);
		if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_TITLE, title, sizeof title) <= 0)
			title[0] = 0;
		page = fz_load_page(ctx, doc, 0);
		r = fz_bound_page(ctx, page);
		s = fz_min(CELL_W / (r.x1 - r.x0), CELL_H / (r.y1 - r.y0));
		pm = fz_new_pixmap_from_page(ctx, page, fz_scale(s, s), fz_device_gray(ctx), 0);
		if (!title[0]) {                       /* fall back to the file name */
			char *dot;
			const char *base = strrchr(name, '/');
			snprintf(title, sizeof title, "%s", base ? base + 1 : name);
			if ((dot = strrchr(title, '.')))
				*dot = 0;
		}
		title[strcspn(title, "\r\n")] = 0;
		if ((f = fopen(path, "wb"))) {
			fprintf(f, "P5\n# %s\n%d %d 255\n", title, pm->w, pm->h);
			for (y = 0; y < pm->h; y++)
				fwrite(pm->samples + y * pm->stride, 1, pm->w, f);
			fclose(f);
		}
	}
	fz_always(ctx) {
		fz_drop_pixmap(ctx, pm);
		fz_drop_page(ctx, page);
		fz_drop_document(ctx, doc);
	}
	fz_catch(ctx)
		logmsg("cover failed for %s: %s", name, fz_caught_message(ctx));
}

static int by_title(const void *a, const void *b)
{
	return strcasecmp(((const struct book *)a)->title, ((const struct book *)b)->title);
}

/* Add the books under dir (relative to USERSTORE_MNT, "" = the root). */
static void scan_dir(const char *dir, int depth, struct book *b, int max, int *n, int *told)
{
	char path[512], rel[256];
	DIR *d;
	struct dirent *e;
	struct stat st;
	int w, h;

	snprintf(path, sizeof path, "%s/%s", USERSTORE_MNT, dir);
	if (!(d = opendir(path)))
		return;
	while ((e = readdir(d)) && *n < max) {
		if (e->d_name[0] == '.' || !strcasecmp(e->d_name, "system"))
			continue;                          /* hidden, .minios, Amazon's index */
		if (snprintf(rel, sizeof rel, "%s%s%s", dir, *dir ? "/" : "", e->d_name) >= (int)sizeof rel)
			continue;
		snprintf(path, sizeof path, "%s/%s", USERSTORE_MNT, rel);
		if (stat(path, &st))
			continue;
		if (S_ISDIR(st.st_mode)) {
			if (depth < SCAN_DEPTH)
				scan_dir(rel, depth + 1, b, max, n, told);
			continue;
		}
		if (!is_book_file(e->d_name))
			continue;
		struct book *k = &b[(*n)++];
		strcpy(k->name, rel);
		if (!cover_read(k->name, k->title, sizeof k->title, &w, &h, NULL)) {
			if (!(*told)++)
				gfx_message("Preparing covers...");
			cover_make(k->name);
			if (!cover_read(k->name, k->title, sizeof k->title, &w, &h, NULL))
				snprintf(k->title, sizeof k->title, "%s", e->d_name);
		}
	}
	closedir(d);
}

int lib_scan(struct book *b, int max)
{
	int n = 0, told = 0;

	scan_dir("", 0, b, max, &n, &told);
	qsort(b, n, sizeof *b, by_title);
	return n;
}

/* Title in the black band: one line, or two split at a space. */
static void band_title(int x, int y, int w, const char *t)
{
	char l1[160];
	int maxw = w - 2 * BAND_PAD, cut = 0, i;

	if (gfx_text_w(t, BAND_PX) <= maxw) {
		gfx_text(x + BAND_PAD, y + (BAND_H + BAND_PX * 2 / 3) / 2, BAND_PX, 255, t);
		return;
	}
	for (i = 0; t[i] && i < (int)sizeof l1; i++) {
		if (t[i] != ' ')
			continue;
		memcpy(l1, t, i);
		l1[i] = 0;
		if (gfx_text_w(l1, BAND_PX) > maxw)
			break;
		cut = i;
	}
	if (!cut) {
		gfx_text_fit(x + BAND_PAD, y + (BAND_H + BAND_PX * 2 / 3) / 2, BAND_PX, 255, t, maxw);
		return;
	}
	memcpy(l1, t, cut);
	l1[cut] = 0;
	gfx_text(x + BAND_PAD, y + BAND_PAD + BAND_PX, BAND_PX, 255, l1);
	gfx_text_fit(x + BAND_PAD, y + BAND_PAD + 2 * BAND_PX + 2, BAND_PX, 255, t + cut + 1, maxw);
}

static void frame(int x, int y, int w, int h, int t, int gray)
{
	gfx_fill(x, y, w, t, gray);
	gfx_fill(x, y + h - t, w, t, gray);
	gfx_fill(x, y, t, h, gray);
	gfx_fill(x + w - t, y, t, h, gray);
}

void lib_draw(struct book *b, int n, int sel)
{
	int first = sel / PER_PAGE * PER_PAGE, i;
	char foot[32];

	gfx_fill(0, 0, SCREEN_W, SCREEN_H, 255);
	gfx_text(GRID_MARGIN, HEADER_H - HEADER_PX / 2, HEADER_PX, 0, HEADER_TEXT);
	gfx_fill(GRID_MARGIN, HEADER_H - 10, SCREEN_W - 2 * GRID_MARGIN, 2, 0);
	if (!n) {
		gfx_text(GRID_MARGIN, HEADER_H + 2 * HEADER_PX, BAND_PX + 4, 0,
		         "No books yet. Connect to a PC and copy");
		gfx_text(GRID_MARGIN, HEADER_H + 2 * HEADER_PX + BAND_PX + 12, BAND_PX + 4, 0,
		         "EPUB or PDF files onto the Kindle drive.");
		return;
	}
	for (i = first; i < n && i < first + PER_PAGE; i++) {
		int c = (i - first) % GRID_COLS, r = (i - first) / GRID_COLS;
		int cx = GRID_MARGIN + c * (CELL_W + GRID_GAP);
		int cy = HEADER_H + r * (CELL_H + GRID_GAP);
		int w = CELL_W, h = CELL_H, x, y;
		uint8_t *px = NULL;
		char t[160];

		if (cover_read(b[i].name, t, sizeof t, &w, &h, &px)) {
			x = cx + (CELL_W - w) / 2;
			y = cy + (CELL_H - h) / 2;
			gfx_blit(px, w, h, w, x, y);
			free(px);
		} else {
			w = CELL_W, h = CELL_H, x = cx, y = cy;
			gfx_fill(x, y, w, h, 200);
		}
		frame(x, y, w, h, 1, 0);
		gfx_fill(x, y + h - BAND_H, w, BAND_H, 0);
		band_title(x, y + h - BAND_H, w, b[i].title);
		if (i == sel)
			frame(x - SEL_BORDER - 1, y - SEL_BORDER - 1,
			      w + 2 * SEL_BORDER + 2, h + 2 * SEL_BORDER + 2, SEL_BORDER, 0);
	}
	snprintf(foot, sizeof foot, "%d / %d", sel / PER_PAGE + 1, (n + PER_PAGE - 1) / PER_PAGE);
	gfx_text((SCREEN_W - gfx_text_w(foot, FOOTER_PX)) / 2, SCREEN_H - GRID_MARGIN, FOOTER_PX, 0, foot);
}
