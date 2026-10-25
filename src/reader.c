/* Full-screen reader. EPUB is reflowed to the screen; PDF pages are
 * cropped, fitted to the width and shown in pieces. Each page is rendered
 * once and kept, and the next page is rendered while the current one is
 * read, so turning forward is only a screen refresh. The page is saved on
 * every page turn. */
#include <stdio.h>
#include <time.h>
#include "minios.h"

/* One rendered page: the window x0,y0 (h tall) of pm is shown in n pieces. */
struct view { fz_location loc; fz_pixmap *pm; int x0, y0, h, n, margin; };

static int same(fz_location a, fz_location b)
{
	return a.chapter == b.chapter && a.page == b.page;
}

static void view_drop(struct view *v)
{
	fz_drop_pixmap(ctx, v->pm);
	v->pm = NULL;
	v->loc = fz_make_location(-1, -1);
}

static int ms_now(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* The part of a PDF page worth showing. Its height covers everything drawn
 * (so figures are never cut); its width only upright text and images, so
 * sideways margin notes such as arXiv stamps do not shrink the page. */
static fz_rect content_box(fz_display_list *dl, fz_rect r)
{
	fz_rect all = fz_empty_rect, text = fz_empty_rect;
	fz_device *dev = NULL;
	fz_stext_page *tp = NULL;

	fz_var(dev);
	fz_var(tp);
	fz_var(text);
	fz_try(ctx) {
		fz_stext_block *b;
		fz_stext_line *l;

		dev = fz_new_bbox_device(ctx, &all);
		fz_run_display_list(ctx, dl, dev, fz_identity, fz_infinite_rect, NULL);
		fz_close_device(ctx, dev);
		tp = fz_new_stext_page_from_display_list(ctx, dl, NULL);
		for (b = tp->first_block; b; b = b->next) {
			if (b->type == FZ_STEXT_BLOCK_IMAGE)
				text = fz_union_rect(text, b->bbox);
			else
				for (l = b->u.t.first_line; l; l = l->next)
					if (l->dir.x > 0.9f)
						text = fz_union_rect(text, l->bbox);
		}
	}
	fz_always(ctx) {
		fz_drop_stext_page(ctx, tp);
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
		all = r;
	all = fz_intersect_rect(all, r);
	if (!fz_is_empty_rect(text)) {
		all.x0 = fz_max(text.x0, r.x0);
		all.x1 = fz_min(text.x1, r.x1);
	}
	if (fz_is_empty_rect(all) || all.x1 - all.x0 < 20 || all.y1 - all.y0 < 20)
		all = r;                               /* blank or odd page: show it whole */
	return all;
}

/* Render page loc into v (pm stays NULL on error). PDF: crop to the
 * content, fit its width, and split tall pages into pieces that overlap by
 * at least PDF_OVERLAP. The page is interpreted once, into a display list. */
static void prepare(fz_document *doc, fz_location loc, int reflow, struct view *v)
{
	fz_page *page = NULL;
	fz_display_list *dl = NULL;

	view_drop(v);
	v->loc = loc;
	v->x0 = v->y0 = 0;
	v->n = 1;
	v->margin = reflow ? READER_MARGIN : PDF_MARGIN;
	fz_var(page);
	fz_var(dl);
	fz_try(ctx) {
		int view = SCREEN_H - 2 * v->margin;
		fz_rect bb;
		float s;

		page = fz_load_chapter_page(ctx, doc, loc.chapter, loc.page);
		dl = fz_new_display_list_from_page(ctx, page);
		if (reflow) {
			v->pm = fz_new_pixmap_from_display_list(ctx, dl, fz_identity, fz_device_gray(ctx), 0);
			v->h = v->pm->h;
		} else {
			bb = content_box(dl, fz_bound_page(ctx, page));
			s = (SCREEN_W - 2 * PDF_MARGIN) / (bb.x1 - bb.x0);
			if ((bb.y1 - bb.y0) * s > view && (bb.y1 - bb.y0) * s <= view * (100 + PDF_SQUEEZE) / 100)
				s = view / (bb.y1 - bb.y0);        /* nearly fits: shrink, don't split */
			v->pm = fz_new_pixmap_from_display_list(ctx, dl, fz_scale(s, s), fz_device_gray(ctx), 0);
			v->x0 = fz_clampi((int)(bb.x0 * s), 0, v->pm->w - 1);
			v->y0 = fz_clampi((int)(bb.y0 * s), 0, v->pm->h - 1);
			v->h = fz_mini((int)((bb.y1 - bb.y0) * s), v->pm->h - v->y0);
			if (v->h > view)
				v->n = (v->h - PDF_OVERLAP + view - PDF_OVERLAP - 1) / (view - PDF_OVERLAP);
		}
	}
	fz_always(ctx) {
		fz_drop_display_list(ctx, dl);
		fz_drop_page(ctx, page);
	}
	fz_catch(ctx)
		logmsg("render %d/%d: %s", loc.chapter, loc.page, fz_caught_message(ctx));
}

/* Draw piece *piece of v (-1 = the last piece; resolved in place). */
static void show(struct view *v, int *piece)
{
	int m = v->margin, view = SCREEN_H - 2 * m, y0 = v->y0;

	gfx_fill(0, 0, SCREEN_W, SCREEN_H, 255);
	if (!v->pm) {
		gfx_text(READER_MARGIN, SCREEN_H / 2, READER_EM, 0, "This page could not be shown.");
		return;
	}
	if (*piece < 0 || *piece >= v->n)
		*piece = *piece < 0 ? v->n - 1 : 0;
	if (v->n > 1)                              /* spread the pieces evenly */
		y0 += (v->h - view) * *piece / (v->n - 1);
	gfx_blit(v->pm->samples + y0 * v->pm->stride + v->x0,
	         fz_mini(v->pm->w - v->x0, SCREEN_W - 2 * m), fz_mini(v->h, view), v->pm->stride,
	         m, v->h < view ? (SCREEN_H - v->h) / 2 : m);
}

/* Returns the key that closed the book: KHOME, KPOWER, or KNONE when the
 * USB cable was plugged in. */
int reader_run(const char *name)
{
	char full[512];
	fz_document *doc = NULL;
	fz_location loc = { 0, 0 }, next;
	struct view cur = { 0 }, ahead = { 0 }, t;
	int turns = -1, reflow = 0, key, redraw = 1, moved = 1, piece = 0, t0 = 0;

	snprintf(full, sizeof full, "%s/%s", USERSTORE_MNT, name);
	pos_get(POS_FILE, name, &loc.chapter, &loc.page);
	fz_var(doc);
	fz_var(reflow);
	fz_var(loc);
	fz_var(next);
	fz_var(cur);
	fz_var(ahead);
	fz_var(turns);
	fz_var(redraw);
	fz_var(moved);
	fz_var(piece);
	fz_var(t0);
	fz_try(ctx) {
		doc = fz_open_document(ctx, full);
		reflow = fz_is_document_reflowable(ctx, doc);
		if (reflow)
			fz_layout_document(ctx, doc, SCREEN_W - 2 * READER_MARGIN,
			                   SCREEN_H - 2 * READER_MARGIN, READER_EM);
		if (loc.chapter < 0 || loc.chapter >= fz_count_chapters(ctx, doc) ||
		    loc.page < 0 || loc.page >= fz_count_chapter_pages(ctx, doc, loc.chapter))
			loc = fz_make_location(0, 0);   /* book changed since last time */
	}
	fz_catch(ctx) {
		logmsg("open %s: %s", name, fz_caught_message(ctx));
		fz_drop_document(ctx, doc);
		gfx_message("This book could not be opened.");
		input_next(3000);
		return KHOME;
	}
	view_drop(&ahead);
	prepare(doc, loc, reflow, &cur);

	for (;;) {
		if (redraw) {
			show(&cur, &piece);
			disp_flush(moved && ++turns % FULL_REFRESH_EVERY == 0);   /* flash on open, then every Nth page */
			if (t0 && ms_now() - t0 > 400)
				logmsg("slow turn %d/%d: %d ms", loc.chapter, loc.page, ms_now() - t0);
			if (moved && pos_put(POS_FILE, name, loc.chapter, loc.page))
				logmsg("could not save position for %s", name);
			moved = 0;
			next = loc;                        /* render the next page while it is read */
			fz_try(ctx)
				next = fz_next_page(ctx, doc, loc);
			fz_catch(ctx)
				logmsg("turn: %s", fz_caught_message(ctx));
			if (!same(next, loc) && !same(next, ahead.loc))
				prepare(doc, next, reflow, &ahead);
		}
		while ((key = input_next(POLL_MS)) == KNONE && !usb_online())
			;
		if (key == KMENU || key == KBACK)
			key = KHOME;
		if (key == KNONE || key == KHOME || key == KPOWER)
			break;
		t0 = ms_now();
		next = loc;
		redraw = 0;
		if (key == KNEXT || key == KRIGHT || key == KDOWN || key == KOK) {
			if (piece < cur.n - 1) {
				piece++;
				redraw = 1;
				continue;
			}
			fz_try(ctx)
				next = fz_next_page(ctx, doc, loc);
			fz_catch(ctx)
				logmsg("turn: %s", fz_caught_message(ctx));
			piece = 0;
		} else if (key == KPREV || key == KLEFT || key == KUP) {
			if (piece > 0) {
				piece--;
				redraw = 1;
				continue;
			}
			fz_try(ctx)
				next = fz_previous_page(ctx, doc, loc);
			fz_catch(ctx)
				logmsg("turn: %s", fz_caught_message(ctx));
			piece = -1;
		}
		redraw = moved = !same(next, loc);
		if (redraw) {
			loc = next;
			t = ahead;                         /* forward: the prepared page; back: */
			ahead = cur;                       /* the page left becomes the next one */
			cur = t;
			if (!same(cur.loc, loc))
				prepare(doc, loc, reflow, &cur);
		} else {
			piece = piece < 0 ? 0 : cur.n - 1; /* at the first/last page: stay */
		}
	}
	view_drop(&cur);
	view_drop(&ahead);
	fz_drop_document(ctx, doc);
	return key;
}
