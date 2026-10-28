/* Chess: legal moves (castling, en passant, promotion), FEN saves, and an
 * alpha-beta AI with quiescence and iterative deepening under a time
 * limit. Low Elo levels play weaker through random moves and noise on the
 * root scores. Squares as in checkers.c: 0 = a8, 63 = h1. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "games.h"
#include "../config.h"

#define WIN      100000
#define MAXPLY   48                            /* checks in quiescence end here */
#define ENDGAME  1300                          /* pieces left (no pawns/kings): king goes central */
#define P CH_PAWN
#define N CH_KNIGHT
#define B CH_BISHOP
#define R CH_ROOK
#define Q CH_QUEEN
#define K CH_KING

static const char glyph[] = "kqrbnp.PNBRQK";   /* piece + 6 */
static const char castles[] = "KQkq";
static const int value[7] = { 0, 100, 320, 330, 500, 900, 0 };
static const int dirs[8][2] = { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 },   /* diagonals */
                                { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };   /* lines */
static const int jumps[8][2] = { { -2, -1 }, { -2, 1 }, { -1, -2 }, { -1, 2 },
                                 { 1, -2 }, { 1, 2 }, { 2, -1 }, { 2, 1 } };

/* Piece-square tables from white's side, a8 first (Michniewski's). */
static const signed char pst[7][64] = {
	{ 0 },
	{  0,  0,  0,  0,  0,  0,  0,  0,  50, 50, 50, 50, 50, 50, 50, 50,
	  10, 10, 20, 30, 30, 20, 10, 10,   5,  5, 10, 25, 25, 10,  5,  5,
	   0,  0,  0, 20, 20,  0,  0,  0,   5, -5,-10,  0,  0,-10, -5,  5,
	   5, 10, 10,-20,-20, 10, 10,  5,   0,  0,  0,  0,  0,  0,  0,  0 },
	{-50,-40,-30,-30,-30,-30,-40,-50, -40,-20,  0,  0,  0,  0,-20,-40,
	 -30,  0, 10, 15, 15, 10,  0,-30, -30,  5, 15, 20, 20, 15,  5,-30,
	 -30,  0, 15, 20, 20, 15,  0,-30, -30,  5, 10, 15, 15, 10,  5,-30,
	 -40,-20,  0,  5,  5,  0,-20,-40, -50,-40,-30,-30,-30,-30,-40,-50 },
	{-20,-10,-10,-10,-10,-10,-10,-20, -10,  0,  0,  0,  0,  0,  0,-10,
	 -10,  0,  5, 10, 10,  5,  0,-10, -10,  5,  5, 10, 10,  5,  5,-10,
	 -10,  0, 10, 10, 10, 10,  0,-10, -10, 10, 10, 10, 10, 10, 10,-10,
	 -10,  5,  0,  0,  0,  0,  5,-10, -20,-10,-10,-10,-10,-10,-10,-20 },
	{  0,  0,  0,  0,  0,  0,  0,  0,   5, 10, 10, 10, 10, 10, 10,  5,
	  -5,  0,  0,  0,  0,  0,  0, -5,  -5,  0,  0,  0,  0,  0,  0, -5,
	  -5,  0,  0,  0,  0,  0,  0, -5,  -5,  0,  0,  0,  0,  0,  0, -5,
	  -5,  0,  0,  0,  0,  0,  0, -5,   0,  0,  0,  5,  5,  0,  0,  0 },
	{-20,-10,-10, -5, -5,-10,-10,-20, -10,  0,  0,  0,  0,  0,  0,-10,
	 -10,  0,  5,  5,  5,  5,  0,-10,  -5,  0,  5,  5,  5,  5,  0, -5,
	   0,  0,  5,  5,  5,  5,  0, -5, -10,  5,  5,  5,  5,  5,  0,-10,
	 -10,  0,  5,  0,  0,  0,  0,-10, -20,-10,-10, -5, -5,-10,-10,-20 },
	{-30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
	 -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
	 -20,-30,-30,-40,-40,-30,-30,-20, -10,-20,-20,-20,-20,-20,-20,-10,
	  20, 20,  0,  0,  0,  0, 20, 20,  20, 30, 10,  0,  0, 10, 30, 20 },
};
static const signed char king_end[64] = {
	-50,-40,-30,-20,-20,-30,-40,-50, -30,-20,-10,  0,  0,-10,-20,-30,
	-30,-10, 20, 30, 30, 20,-10,-30, -30,-10, 30, 40, 40, 30,-10,-30,
	-30,-10, 30, 40, 40, 30,-10,-30, -30,-10, 20, 30, 30, 20,-10,-30,
	-30,-30,  0,  0,  0,  0,-30,-30, -50,-30,-30,-30,-30,-30,-30,-50 };

static int on(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

/* Is s attacked by side `by`? */
static int attacked(const struct chess *g, int s, int by)
{
	int r = s / 8, c = s % 8, k, i, rr, cc, p;

	for (k = -1; k <= 1; k += 2)               /* pawns of `by` stand one row behind s */
		if (on(r + by, c + k) && g->b[(r + by) * 8 + c + k] == by * P)
			return 1;
	for (k = 0; k < 8; k++) {
		rr = r + jumps[k][0], cc = c + jumps[k][1];
		if (on(rr, cc) && g->b[rr * 8 + cc] == by * N)
			return 1;
	}
	for (k = 0; k < 8; k++)
		for (i = 1, rr = r + dirs[k][0], cc = c + dirs[k][1]; on(rr, cc);
		     i++, rr += dirs[k][0], cc += dirs[k][1]) {
			if (!(p = g->b[rr * 8 + cc] * by))
				continue;
			if (p == Q || (p == K && i == 1) || p == (k < 4 ? B : R))
				return 1;
			break;                             /* blocked */
		}
	return 0;
}

static int king(const struct chess *g, int side)
{
	int s;

	for (s = 0; s < 63 && g->b[s] != side * K; s++)
		;
	return s;
}

int ch_in_check(const struct chess *g)
{
	return attacked(g, king(g, g->turn), -g->turn);
}

static int add(struct cmove *out, int n, int from, int to, int promo)
{
	out[n].from = from, out[n].to = to, out[n].promo = promo;
	return n + 1;
}

/* Every move that is legal except for leaving the own king attacked. */
static int pseudo(const struct chess *g, struct cmove *out)
{
	int s, t, p, r, c, k, rr, cc, q, n = 0, u = g->turn, fwd = -u;

	for (s = 0; s < 64; s++) {
		if ((p = g->b[s] * u) <= 0)
			continue;
		r = s / 8, c = s % 8;
		if (p == P) {
			for (k = -1; k <= 1; k++) {
				if (!on(r + fwd, c + k))
					continue;
				t = (r + fwd) * 8 + c + k;
				if (k ? !(g->b[t] * u < 0 || t == g->ep) : g->b[t] != 0)
					continue;
				if (t / 8 == (u > 0 ? 0 : 7))
					for (q = Q; q >= N; q--)
						n = add(out, n, s, t, q);
				else
					n = add(out, n, s, t, 0);
			}
			if (r == (u > 0 ? 6 : 1) && !g->b[s + 8 * fwd] && !g->b[s + 16 * fwd])
				n = add(out, n, s, s + 16 * fwd, 0);
		} else if (p == N) {
			for (k = 0; k < 8; k++) {
				rr = r + jumps[k][0], cc = c + jumps[k][1];
				if (on(rr, cc) && g->b[rr * 8 + cc] * u <= 0)
					n = add(out, n, s, rr * 8 + cc, 0);
			}
		} else {
			for (k = p == R ? 4 : 0; k < (p == B ? 4 : 8); k++)
				for (rr = r + dirs[k][0], cc = c + dirs[k][1]; on(rr, cc);
				     rr += dirs[k][0], cc += dirs[k][1]) {
					t = rr * 8 + cc;
					if (g->b[t] * u > 0)
						break;
					n = add(out, n, s, t, 0);
					if (g->b[t] || p == K)
						break;
				}
		}
	}
	/* castling: not out of or through check (into check is caught later) */
	s = (u > 0 ? 7 : 0) * 8 + 4;
	if ((g->castle & (u > 0 ? 1 : 4)) && !g->b[s + 1] && !g->b[s + 2] &&
	    !attacked(g, s, -u) && !attacked(g, s + 1, -u))
		n = add(out, n, s, s + 2, 0);
	if ((g->castle & (u > 0 ? 2 : 8)) && !g->b[s - 1] && !g->b[s - 2] && !g->b[s - 3] &&
	    !attacked(g, s, -u) && !attacked(g, s - 1, -u))
		n = add(out, n, s, s - 2, 0);
	return n;
}

/* Castling rights lost when a piece leaves or lands on s. */
static int rights(int s)
{
	switch (s) {
	case 60: return 3;
	case 63: return 1;
	case 56: return 2;
	case 4:  return 12;
	case 7:  return 4;
	case 0:  return 8;
	}
	return 0;
}

void ch_play(struct chess *g, struct cmove m)
{
	int p = g->b[m.from], u = g->turn, a = abs(p), rf;

	g->half = a == P || g->b[m.to] ? 0 : g->half + 1;
	if (a == P && m.to == g->ep)
		g->b[m.to + 8 * u] = 0;                /* en passant: the pawn beside us */
	if (a == K && abs(m.to - m.from) == 2) {   /* castling: the rook jumps over */
		rf = m.to > m.from ? m.from + 3 : m.from - 4;
		g->b[(m.from + m.to) / 2] = g->b[rf];
		g->b[rf] = 0;
	}
	g->b[m.to] = m.promo ? u * m.promo : p;
	g->b[m.from] = 0;
	g->ep = a == P && abs(m.to - m.from) == 16 ? (m.from + m.to) / 2 : -1;
	g->castle &= ~(rights(m.from) | rights(m.to));
	if (u < 0)
		g->full++;
	g->turn = -u;
}

/* ponytail: copy-make legality check on every move; make/unmake if the
 * device needs more depth. */
int ch_moves(const struct chess *g, struct cmove *out)
{
	struct cmove all[CH_MAXMOVES];
	struct chess t;
	int i, n = pseudo(g, all), k = 0;

	for (i = 0; i < n; i++) {
		t = *g;
		ch_play(&t, all[i]);
		if (!attacked(&t, king(&t, g->turn), -g->turn))
			out[k++] = all[i];
	}
	return k;
}

/* ponytail: no threefold repetition; the 50-move rule still ends loops. */
int ch_result(const struct chess *g)
{
	struct cmove m[CH_MAXMOVES];
	int s, minors = 0, other = 0;

	if (!ch_moves(g, m))
		return ch_in_check(g) ? CH_MATE : CH_STALEMATE;
	if (g->half >= 100)
		return CH_DRAW;
	for (s = 0; s < 64; s++)
		switch (abs(g->b[s])) {
		case N: case B:         minors++; break;
		case P: case R: case Q: other++; break;
		}
	return !other && minors <= 1 ? CH_DRAW : CH_PLAYING;
}

/* Material and piece-square tables, from white's side. */
static int eval(const struct chess *g)
{
	int s, p, a, sq, v = 0, mat = 0;

	for (s = 0; s < 64; s++)
		if ((a = abs(g->b[s])) > P && a < K)
			mat += value[a];
	for (s = 0; s < 64; s++) {
		if (!(p = g->b[s]))
			continue;
		a = abs(p);
		sq = p > 0 ? s : (7 - s / 8) * 8 + s % 8;
		a = value[a] + (a == K && mat <= ENDGAME ? king_end[sq] : pst[a][sq]);
		v += p > 0 ? a : -a;
	}
	return v;
}

/* Captures (most valuable victim, cheapest attacker) and promotions
 * first. With captures_only, quiet moves are dropped. */
static int order(const struct chess *g, struct cmove *m, int n, int captures_only)
{
	int sc[CH_MAXMOVES], i, j, k = 0, v, s;
	struct cmove t;

	for (i = 0; i < n; i++) {
		t = m[i];
		v = abs(g->b[t.to]);
		if (!v && abs(g->b[t.from]) == P && t.from % 8 != t.to % 8)
			v = P;                             /* en passant */
		if (captures_only && !v && !t.promo)
			continue;
		s = (v ? 10 * value[v] - abs(g->b[t.from]) : 0) + (t.promo ? value[(int)t.promo] : 0);
		for (j = k++; j > 0 && sc[j - 1] < s; j--)
			m[j] = m[j - 1], sc[j] = sc[j - 1];
		m[j] = t, sc[j] = s;
	}
	return k;
}

static long deadline;
static int aborted;
static unsigned long nodes;

/* Score for the side to move. depth <= 0 is the quiescence search:
 * captures only, unless in check. */
static int search(const struct chess *g, int depth, int a, int b, int ply)
{
	struct cmove m[CH_MAXMOVES];
	struct chess t;
	int n, i, v, best = -WIN - 1, legal = 0, check, quiet;

	if ((++nodes & 1023) == 0 && mono_ms() > deadline)
		aborted = 1;
	if (aborted)
		return 0;
	if (g->half >= 100)
		return 0;
	if (ply >= MAXPLY)
		return eval(g) * g->turn;
	check = ch_in_check(g);
	if ((quiet = depth <= 0 && !check)) {      /* stand pat */
		if ((best = eval(g) * g->turn) >= b)
			return best;
		if (best > a)
			a = best;
	}
	n = pseudo(g, m);
	n = order(g, m, n, quiet);
	for (i = 0; i < n; i++) {
		t = *g;
		ch_play(&t, m[i]);
		if (attacked(&t, king(&t, g->turn), -g->turn))
			continue;
		legal++;
		v = -search(&t, depth - 1, -b, -a, ply + 1);
		if (v > best)
			best = v;
		if (best > a)
			a = best;
		if (a >= b)
			break;
	}
	if (!legal && !quiet)
		return check ? -WIN + ply : 0;         /* mated (later is better) or stalemate */
	return best;
}

struct cmove ch_ai(const struct chess *g, int depth, int random_pct, int noise, long ms, int *reached)
{
	struct cmove m[CH_MAXMOVES], best, t;
	struct chess c;
	int n = ch_moves(g, m), d, i, v, a, cur;

	*reached = 0;
	if (!n) {
		best.from = best.to = -1, best.promo = 0;
		return best;
	}
	if (n == 1 || (int)(rng() % 100) < random_pct)
		return m[n == 1 ? 0 : rng() % n];
	n = order(g, m, n, 0);
	deadline = mono_ms() + ms;
	aborted = 0;
	best = m[0];
	for (d = 1; d <= depth; d++) {
		a = -WIN - 1;
		cur = 0;
		for (i = 0; i < n && !aborted; i++) {
			c = *g;
			ch_play(&c, m[i]);
			/* with noise every root move needs its true score: no cut-off */
			v = -search(&c, d - 1, -WIN - 1, noise ? WIN + 1 : -a, 1);
			if (noise)
				v += (int)(rng() % (2 * noise + 1)) - noise;
			if (!aborted && v > a)
				a = v, cur = i;
		}
		if (aborted)
			break;                             /* keep the last full depth */
		best = m[cur];
		*reached = d;
		t = m[0], m[0] = m[cur], m[cur] = t;   /* best first next time */
		if (a > WIN - 1000)
			break;                             /* shortest mate found */
	}
	return best;
}

void ch_fen(const struct chess *g, char *out)
{
	int r, c, e, i;
	char *o = out;

	for (r = 0; r < 8; r++) {
		for (c = e = 0; c < 8; c++) {
			if (!g->b[r * 8 + c]) {
				e++;
				continue;
			}
			if (e)
				*o++ = '0' + e, e = 0;
			*o++ = glyph[g->b[r * 8 + c] + 6];
		}
		if (e)
			*o++ = '0' + e;
		if (r < 7)
			*o++ = '/';
	}
	*o++ = ' ', *o++ = g->turn > 0 ? 'w' : 'b', *o++ = ' ';
	if (!g->castle)
		*o++ = '-';
	for (i = 0; i < 4; i++)
		if (g->castle & 1 << i)
			*o++ = castles[i];
	*o++ = ' ';
	if (g->ep < 0)
		*o++ = '-';
	else
		*o++ = 'a' + g->ep % 8, *o++ = '8' - g->ep / 8;
	sprintf(o, " %d %d", g->half, g->full);
}

int ch_set_fen(struct chess *g, const char *fen)
{
	static const int ksq[4] = { 60, 60, 4, 4 }, rsq[4] = { 63, 56, 7, 0 };
	struct chess t;
	char place[80], side, cas[8], ep[4];
	const char *p, *q;
	int r = 0, c = 0, s, wk = 0, bk = 0, i;

	memset(&t, 0, sizeof t);
	if (sscanf(fen, " %79s %c %7s %3s %d %d", place, &side, cas, ep, &t.half, &t.full) != 6 ||
	    (side != 'w' && side != 'b') || t.half < 0 || t.half > 150 || t.full < 1 || t.full > 9999)
		return -1;
	for (p = place; *p; p++) {
		if (*p == '/') {
			if (c != 8 || ++r > 7)
				return -1;
			c = 0;
		} else if (*p >= '1' && *p <= '8') {
			if ((c += *p - '0') > 8)
				return -1;
		} else if (*p != '.' && (q = strchr(glyph, *p)) && c < 8)
			t.b[r * 8 + c++] = (q - glyph) - 6;
		else
			return -1;
	}
	if (r != 7 || c != 8)
		return -1;
	for (s = 0; s < 64; s++) {
		wk += t.b[s] == K, bk += t.b[s] == -K;
		if (abs(t.b[s]) == P && (s < 8 || s >= 56))
			return -1;                         /* a pawn on the first or last rank */
	}
	if (wk != 1 || bk != 1)
		return -1;
	t.turn = side == 'w' ? 1 : -1;
	if (strcmp(cas, "-"))
		for (p = cas; *p; p++) {
			if (!(q = strchr(castles, *p)))
				return -1;
			t.castle |= 1 << (q - castles);
		}
	for (i = 0; i < 4; i++)                    /* a right needs king and rook at home */
		if (t.castle & 1 << i &&
		    (t.b[ksq[i]] != (i < 2 ? K : -K) || t.b[rsq[i]] != (i < 2 ? R : -R)))
			return -1;
	t.ep = -1;
	if (strcmp(ep, "-")) {
		if (strlen(ep) != 2 || ep[0] < 'a' || ep[0] > 'h' || ep[1] != (t.turn > 0 ? '6' : '3'))
			return -1;
		t.ep = ('8' - ep[1]) * 8 + ep[0] - 'a';
		if (t.b[t.ep] || t.b[t.ep + 8 * t.turn] != -t.turn * P)
			return -1;                         /* no pawn that just moved two */
	}
	if (attacked(&t, king(&t, -t.turn), t.turn))
		return -1;                             /* the side not to move is in check */
	*g = t;
	return 0;
}

void ch_new(struct chess *g)
{
	ch_set_fen(g, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void ch_save(const struct chess *g, int level, int color, char *buf, int n)
{
	char f[CH_FEN_MAX];

	ch_fen(g, f);
	snprintf(buf, n, "minios-chess 1\nlevel %d\ncolor %d\nfen %s\n", level, color, f);
}

int ch_load(struct chess *g, int *level, int *color, const char *text)
{
	static const int elo[] = CHESS_ELO;
	int lv, col, k = 0;

	if (sscanf(text, " minios-chess 1 level %d color %d fen%n", &lv, &col, &k) != 2 || !k ||
	    lv < 0 || lv >= (int)(sizeof elo / sizeof *elo) || (col != 1 && col != -1) ||
	    ch_set_fen(g, text + k))
		return -1;
	*level = lv;
	*color = col;
	return 0;
}
