/* Sudoku rules: a bitmask solver that counts solutions, a generator that
 * removes clues while the solution stays unique, and the save format. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "games.h"
#include "../config.h"

#define BOX(i) ((i) / 27 * 3 + (i) % 9 / 3)

static uint32_t seed = 1;

void rng_seed(uint32_t s) { seed = s ? s : 1; }

uint32_t rng(void)                             /* xorshift32 */
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

long mono_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000L + t.tv_nsec / 1000000;
}

static void shuffle(uint8_t *a, int n)
{
	int i, j;
	uint8_t t;

	for (i = n - 1; i > 0; i--) {
		j = rng() % (i + 1);
		t = a[i], a[i] = a[j], a[j] = t;
	}
}

/* Fill the most constrained empty cell first. Counts solutions up to
 * limit; with fill, digits are tried in random order and the first
 * solution is left in g. */
static int search(uint8_t *g, uint16_t *row, uint16_t *col, uint16_t *box, int limit, int fill)
{
	int i, best = -1, bestn = 10, found = 0, k, n;
	uint16_t m, bm = 0;
	uint8_t d[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

	for (i = 0; i < 81; i++) {
		if (g[i])
			continue;
		m = 0x3fe & ~(row[i / 9] | col[i % 9] | box[BOX(i)]);
		n = __builtin_popcount(m);
		if (n < bestn) {
			best = i, bestn = n, bm = m;
			if (!n)
				return 0;
		}
	}
	if (best < 0)
		return 1;
	if (fill)
		shuffle(d, 9);
	for (k = 0; k < 9; k++) {
		if (!(bm >> d[k] & 1))
			continue;
		g[best] = d[k];
		row[best / 9] |= 1 << d[k], col[best % 9] |= 1 << d[k], box[BOX(best)] |= 1 << d[k];
		found += search(g, row, col, box, limit, fill);
		if (found >= limit && fill)
			return found;
		g[best] = 0;
		row[best / 9] &= ~(1 << d[k]), col[best % 9] &= ~(1 << d[k]), box[BOX(best)] &= ~(1 << d[k]);
		if (found >= limit)
			return found;
	}
	return found;
}

int sdk_count(const uint8_t *g, int limit)
{
	uint8_t t[81];
	uint16_t row[9] = { 0 }, col[9] = { 0 }, box[9] = { 0 }, bit;
	int i;

	memcpy(t, g, 81);
	for (i = 0; i < 81; i++) {
		if (!t[i])
			continue;
		if (t[i] > 9)
			return 0;
		bit = 1 << t[i];
		if ((row[i / 9] | col[i % 9] | box[BOX(i)]) & bit)
			return 0;                          /* the givens already clash */
		row[i / 9] |= bit, col[i % 9] |= bit, box[BOX(i)] |= bit;
	}
	return search(t, row, col, box, limit, 0);
}

int sdk_new(struct sudoku *s, int level)
{
	static const int clues[] = SDK_CLUES;
	uint8_t sol[81], p[81], order[81], t;
	uint16_t row[9], col[9], box[9];
	long end = mono_ms() + SDK_GEN_MS;
	int target = clues[level], best = 82, n, k, i;

	do {
		memset(sol, 0, 81);
		memset(row, 0, sizeof row), memset(col, 0, sizeof col), memset(box, 0, sizeof box);
		search(sol, row, col, box, 1, 1);
		memcpy(p, sol, 81);
		for (i = 0; i < 81; i++)
			order[i] = i;
		shuffle(order, 81);
		for (n = 81, k = 0; k < 81 && n > target; k++) {
			t = p[order[k]];
			p[order[k]] = 0;
			if (sdk_count(p, 2) == 1)
				n--;
			else
				p[order[k]] = t;               /* needed for a unique solution */
		}
		if (n < best) {
			best = n;
			memcpy(s->sol, sol, 81);
			memcpy(s->grid, p, 81);
		}
	} while (best > target + SDK_CLUE_SLACK && mono_ms() < end);
	for (i = 0; i < 81; i++)
		s->given[i] = s->grid[i] != 0;
	memset(s->wrong, 0, 81);
	s->level = level;
	s->secs = 0;
	return best;
}

int sdk_conflict(const struct sudoku *s, int i)
{
	int j, d = s->grid[i];

	if (d)
		for (j = 0; j < 81; j++)
			if (j != i && s->grid[j] == d &&
			    (j / 9 == i / 9 || j % 9 == i % 9 || BOX(j) == BOX(i)))
				return 1;
	return 0;
}

int sdk_solved(const struct sudoku *s)
{
	return !memcmp(s->grid, s->sol, 81);
}

void sdk_save(const struct sudoku *s, char *buf, int n)
{
	char g[82], gv[82], so[82], w[82];
	int i;

	for (i = 0; i < 81; i++) {
		g[i] = '0' + s->grid[i];
		gv[i] = '0' + s->given[i];
		so[i] = '0' + s->sol[i];
		w[i] = '0' + s->wrong[i];
	}
	g[81] = gv[81] = so[81] = w[81] = 0;
	snprintf(buf, n, "minios-sudoku 1\nlevel %d\nsecs %d\nconflicts %d\n"
	         "grid %s\ngiven %s\nsol %s\nwrong %s\n",
	         s->level, s->secs, s->show_conflicts, g, gv, so, w);
}

int sdk_load(struct sudoku *s, const char *text)
{
	char g[82], gv[82], so[82], w[82];
	struct sudoku t;
	int i;

	if (sscanf(text, " minios-sudoku 1 level %d secs %d conflicts %d grid %81s given %81s sol %81s wrong %81s",
	           &t.level, &t.secs, &t.show_conflicts, g, gv, so, w) != 7 ||
	    strlen(g) != 81 || strlen(gv) != 81 || strlen(so) != 81 || strlen(w) != 81 ||
	    t.level < 0 || t.level > 2 || t.secs < 0 || (t.show_conflicts & ~1))
		return -1;
	for (i = 0; i < 81; i++) {
		if (g[i] < '0' || g[i] > '9' || so[i] < '1' || so[i] > '9' ||
		    (gv[i] != '0' && gv[i] != '1') || (w[i] != '0' && w[i] != '1'))
			return -1;
		t.grid[i] = g[i] - '0';
		t.sol[i] = so[i] - '0';
		t.given[i] = gv[i] - '0';
		t.wrong[i] = w[i] - '0';
		if (t.given[i] && t.grid[i] != t.sol[i])
			return -1;
	}
	if (sdk_count(t.sol, 2) != 1)              /* the solution must be a valid full grid */
		return -1;
	*s = t;
	return 0;
}
