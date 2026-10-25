/* English draughts: rules one step (single move or single jump) at a time,
 * so the screen and the AI share them, plus an alpha-beta AI with
 * iterative deepening under a time limit. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "games.h"

#define WIN 100000

static const int dirs[4][2] = { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } };
static const char glyph[] = "Bb.wW";           /* piece value + 2 */

void ck_new(struct ck *g)
{
	int s;

	for (s = 0; s < 64; s++)
		g->b[s] = (s / 8 + s % 8) % 2 == 0 ? 0 : s / 8 < 3 ? -1 : s / 8 > 4 ? 1 : 0;
	g->turn = 1;
	g->chain = -1;
}

/* Jumps (or plain moves) of the piece on s. Men only go forward. */
static int piece_steps(const struct ck *g, int s, int jumps, struct step *out)
{
	int p = g->b[s], r = s / 8, c = s % 8, k, n = 0, dr, dc, m, t;

	for (k = 0; k < 4; k++) {
		dr = dirs[k][0], dc = dirs[k][1];
		if (abs(p) == 1 && dr != -p)           /* white men move up, black men down */
			continue;
		if (jumps) {
			if (r + 2 * dr < 0 || r + 2 * dr > 7 || c + 2 * dc < 0 || c + 2 * dc > 7)
				continue;
			m = (r + dr) * 8 + c + dc, t = (r + 2 * dr) * 8 + c + 2 * dc;
			if (g->b[m] * p < 0 && !g->b[t])
				out[n].from = s, out[n].to = t, out[n++].cap = m;
		} else {
			if (r + dr < 0 || r + dr > 7 || c + dc < 0 || c + dc > 7)
				continue;
			t = (r + dr) * 8 + c + dc;
			if (!g->b[t])
				out[n].from = s, out[n].to = t, out[n++].cap = -1;
		}
	}
	return n;
}

int ck_steps(const struct ck *g, struct step *out)
{
	int s, n = 0;

	if (g->chain >= 0)
		return piece_steps(g, g->chain, 1, out);
	for (s = 0; s < 64; s++)                   /* captures are forced */
		if (g->b[s] * g->turn > 0)
			n += piece_steps(g, s, 1, out + n);
	if (n)
		return n;
	for (s = 0; s < 64; s++)
		if (g->b[s] * g->turn > 0)
			n += piece_steps(g, s, 0, out + n);
	return n;
}

int ck_play(struct ck *g, struct step s)
{
	struct step more[4];
	int p = g->b[s.from];

	g->b[s.to] = p;
	g->b[s.from] = 0;
	if (s.cap >= 0)
		g->b[s.cap] = 0;
	if (abs(p) == 1 && s.to / 8 == (p > 0 ? 0 : 7))
		g->b[s.to] = 2 * p;                    /* crowned: the move ends here */
	else if (s.cap >= 0 && piece_steps(g, s.to, 1, more)) {
		g->chain = s.to;
		return 1;
	}
	g->chain = -1;
	g->turn = -g->turn;
	return 0;
}

/* Material, advancement of men, centre control; from white's side. */
static int eval(const struct ck *g)
{
	int s, v = 0, p, r, c, sc;

	for (s = 0; s < 64; s++) {
		if (!(p = g->b[s]))
			continue;
		r = s / 8, c = s % 8;
		sc = abs(p) == 2 ? 160 : 100 + 4 * (p > 0 ? 7 - r : r);
		if (r >= 2 && r <= 5 && c >= 2 && c <= 5)
			sc += 6;
		v += p > 0 ? sc : -sc;
	}
	return v;
}

static long deadline;
static int aborted;
static unsigned long nodes;

/* Score for the side to move. A jump chain stays with the same side and
 * the same depth; forced captures are searched past depth 0. */
static int negamax(const struct ck *g, int depth, int a, int b, int ply)
{
	struct step st[CK_MAXSTEPS];
	struct ck t;
	int n, i, v, best = -WIN - 1;

	if ((++nodes & 1023) == 0 && mono_ms() > deadline)
		aborted = 1;
	if (aborted)
		return 0;
	if (!(n = ck_steps(g, st)))
		return -WIN + ply;                     /* no move: lost, later is better */
	if (depth <= 0 && g->chain < 0 && st[0].cap < 0)
		return eval(g) * g->turn;
	for (i = 0; i < n; i++) {
		t = *g;
		v = ck_play(&t, st[i]) ? negamax(&t, depth, a, b, ply + 1)
		                       : -negamax(&t, depth - 1, -b, -a, ply + 1);
		if (v > best)
			best = v;
		if (best > a)
			a = best;
		if (a >= b)
			break;
	}
	return best;
}

struct step ck_ai(const struct ck *g, int depth, int random_pct, long ms, int *reached)
{
	struct step st[CK_MAXSTEPS], best, t;
	struct ck c;
	int n = ck_steps(g, st), d, i, v, a, cur;

	*reached = 0;
	if (!n) {
		best.from = best.to = best.cap = -1;
		return best;
	}
	if (n == 1 || (int)(rng() % 100) < random_pct)
		return st[n == 1 ? 0 : rng() % n];
	deadline = mono_ms() + ms;
	aborted = 0;
	best = st[0];
	for (d = 1; d <= depth; d++) {
		a = -WIN - 1;
		cur = 0;
		for (i = 0; i < n && !aborted; i++) {
			c = *g;
			v = ck_play(&c, st[i]) ? negamax(&c, d, a, WIN + 1, 1)
			                       : -negamax(&c, d - 1, -WIN - 1, -a, 1);
			if (!aborted && v > a)
				a = v, cur = i;
		}
		if (aborted)
			break;                             /* keep the last full depth */
		best = st[cur];
		*reached = d;
		t = st[0], st[0] = st[cur], st[cur] = t;   /* best first next time */
	}
	return best;
}

void ck_save(const struct ck *g, int level, char *buf, int n)
{
	char b[65];
	int s;

	for (s = 0; s < 64; s++)
		b[s] = glyph[g->b[s] + 2];
	b[64] = 0;
	snprintf(buf, n, "minios-checkers 1\nlevel %d\nturn %d\nchain %d\nboard %s\n",
	         level, g->turn, g->chain, b);
}

int ck_load(struct ck *g, int *level, const char *text)
{
	char b[65];
	const char *p;
	struct ck t;
	struct step more[4];
	int lv, s, nw = 0, nb = 0;

	if (sscanf(text, " minios-checkers 1 level %d turn %d chain %d board %64s",
	           &lv, &t.turn, &t.chain, b) != 4 || strlen(b) != 64 ||
	    lv < 0 || lv > 2 || (t.turn != 1 && t.turn != -1) || t.chain < -1 || t.chain > 63)
		return -1;
	for (s = 0; s < 64; s++) {
		if (!(p = strchr(glyph, b[s])))
			return -1;
		t.b[s] = (p - glyph) - 2;
		if (t.b[s] && (s / 8 + s % 8) % 2 == 0)
			return -1;                         /* pieces only on dark squares */
		if ((t.b[s] == 1 && s / 8 == 0) || (t.b[s] == -1 && s / 8 == 7))
			return -1;                         /* a man on the far row is a king */
		nw += t.b[s] > 0, nb += t.b[s] < 0;
	}
	if (nw > 12 || nb > 12 ||
	    (t.chain >= 0 && (t.b[t.chain] * t.turn <= 0 || !piece_steps(&t, t.chain, 1, more))))
		return -1;                             /* a jump chain needs a jump left */
	*g = t;
	*level = lv;
	return 0;
}
