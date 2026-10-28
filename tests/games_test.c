/* make test: Sudoku generator/saves, Checkers and Chess rules/AI/saves. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/games.h"

static int sq(int r, int c) { return r * 8 + c; }

static void sudoku(void)
{
	static const char *names[] = { "easy", "medium", "hard" };
	struct sudoku s, t;
	char buf[1024];
	int lv, k, i, clues, worst, lo, hi;
	long t0;

	for (lv = 0; lv < 3; lv++) {
		worst = 0, lo = 81, hi = 0;
		for (k = 0; k < 50; k++) {
			t0 = mono_ms();
			clues = sdk_new(&s, lv);
			if (mono_ms() - t0 > worst)
				worst = mono_ms() - t0;
			lo = clues < lo ? clues : lo, hi = clues > hi ? clues : hi;
			assert(sdk_count(s.grid, 2) == 1);              /* unique */
			assert(sdk_count(s.sol, 2) == 1);
			for (i = 0, clues = 0; i < 81; i++) {
				assert(!s.grid[i] || s.grid[i] == s.sol[i]);
				clues += s.given[i];
			}
		}
		printf("sudoku %s: clues %d..%d, worst %d ms\n", names[lv], lo, hi, worst);
	}
	/* conflicts and solving */
	sdk_new(&s, 0);
	for (i = 0; s.given[i]; i++)
		;
	s.grid[i] = s.sol[i] % 9 + 1;              /* a wrong digit ... */
	s.grid[(i / 9) * 9 + (i % 9 + 1) % 9] = s.grid[i];   /* ... repeated in its row */
	assert(sdk_conflict(&s, i) && !sdk_solved(&s));
	memcpy(s.grid, s.sol, 81);
	assert(sdk_solved(&s) && !sdk_conflict(&s, 0));
	/* saves: round trip, and every kind of damage is refused */
	sdk_new(&s, 2);
	s.grid[3] = s.given[3] ? s.grid[3] : 5;
	s.secs = 77, s.show_conflicts = 1;
	sdk_save(&s, buf, sizeof buf);
	assert(sdk_load(&t, buf) == 0 && !memcmp(&s, &t, sizeof s));
	assert(sdk_load(&t, "") && sdk_load(&t, "garbage\n"));
	buf[strlen(buf) / 2] = 0;
	assert(sdk_load(&t, buf));                 /* truncated */
	sdk_save(&s, buf, sizeof buf);
	*strstr(buf, "sol ") = 'x';
	assert(sdk_load(&t, buf));
	sdk_save(&s, buf, sizeof buf);
	strstr(buf, "sol ")[4] = strstr(buf, "sol ")[5];   /* not a valid grid any more */
	assert(sdk_load(&t, buf));
	sdk_save(&s, buf, sizeof buf);
	strstr(buf, "level ")[6] = '7';
	assert(sdk_load(&t, buf));
}

static void checkers(void)
{
	struct ck g, t;
	struct step st[CK_MAXSTEPS], m;
	char buf[256];
	int n, i, games, ply, lv, reached;
	long t0, worst = 0;

	ck_new(&g);
	assert(ck_steps(&g, st) == 7 && g.turn == 1);   /* 7 opening moves for white */

	/* forced capture: the plain moves of the other man are not allowed */
	memset(&g, 0, sizeof g), g.turn = 1, g.chain = -1;
	g.b[sq(5, 2)] = 1, g.b[sq(4, 3)] = -1, g.b[sq(5, 6)] = 1;
	assert(ck_steps(&g, st) == 1 && st[0].cap == sq(4, 3) && st[0].to == sq(3, 4));

	/* double jump: the same piece goes on, then the turn passes */
	g.b[sq(2, 5)] = -1;
	assert(ck_steps(&g, st) == 1);
	assert(ck_play(&g, st[0]) == 1 && g.chain == sq(3, 4) && g.turn == 1);
	assert(ck_steps(&g, st) == 1 && st[0].to == sq(1, 6));
	assert(ck_play(&g, st[0]) == 0 && g.chain == -1 && g.turn == -1);
	assert(!g.b[sq(4, 3)] && !g.b[sq(2, 5)] && g.b[sq(1, 6)] == 1);

	/* crowning ends the move even when the new king could jump on */
	memset(&g, 0, sizeof g), g.turn = 1, g.chain = -1;
	g.b[sq(2, 1)] = 1, g.b[sq(1, 2)] = -1, g.b[sq(1, 4)] = -1;
	assert(ck_steps(&g, st) == 1 && ck_play(&g, st[0]) == 0);
	assert(g.b[sq(0, 3)] == 2 && g.turn == -1);

	/* kings move both ways; black men only down */
	memset(&g, 0, sizeof g), g.turn = 1, g.chain = -1;
	g.b[sq(4, 3)] = 2;
	assert(ck_steps(&g, st) == 4);
	g.turn = -1, g.b[sq(4, 3)] = -1;
	assert(ck_steps(&g, st) == 2 && st[0].to / 8 == 5 && st[1].to / 8 == 5);

	/* no pieces or no moves: the side to move has lost */
	memset(&g, 0, sizeof g), g.turn = -1, g.chain = -1;
	g.b[sq(4, 3)] = 1;
	assert(ck_steps(&g, st) == 0);

	/* the AI always plays a legal step; whole games end; time limit holds */
	for (games = 0; games < 4; games++) {
		ck_new(&g);
		for (ply = 0; ply < 400 && (n = ck_steps(&g, st)); ply++) {
			t0 = mono_ms();
			m = ck_ai(&g, games < 2 ? 6 : 4, games == 3 ? 20 : 0, 1500, &reached);
			if (mono_ms() - t0 > worst)
				worst = mono_ms() - t0;
			for (i = 0; i < n && memcmp(&st[i], &m, sizeof m); i++)
				;
			assert(i < n);
			ck_play(&g, m);
		}
		printf("checkers AI game %d: %d plies, %s\n", games, ply,
		       n ? "unfinished" : g.turn > 0 ? "black won" : "white won");
	}
	printf("checkers AI: worst %ld ms\n", worst);

	/* saves */
	ck_new(&g);
	ck_steps(&g, st);
	ck_play(&g, st[2]);
	ck_save(&g, 1, buf, sizeof buf);
	assert(ck_load(&t, &lv, buf) == 0 && lv == 1 && !memcmp(&g, &t, sizeof g));
	assert(ck_load(&t, &lv, "") && ck_load(&t, &lv, "minios-checkers 1\n"));
	strstr(buf, "board ")[6] = 'w';            /* piece on a light square */
	assert(ck_load(&t, &lv, buf));
	ck_save(&g, 1, buf, sizeof buf);
	strstr(buf, "board ")[7] = 'x';
	assert(ck_load(&t, &lv, buf));
	ck_save(&g, 1, buf, sizeof buf);
	strstr(buf, "chain ")[6] = '9';            /* chain on an empty square */
	assert(ck_load(&t, &lv, buf));
	buf[20] = 0;
	assert(ck_load(&t, &lv, buf));
}

static long perft(const struct chess *g, int depth)
{
	struct cmove m[CH_MAXMOVES];
	struct chess t;
	int n = ch_moves(g, m), i;
	long sum = 0;

	if (depth == 1)
		return n;
	for (i = 0; i < n; i++) {
		t = *g;
		ch_play(&t, m[i]);
		sum += perft(&t, depth - 1);
	}
	return sum;
}

static void chess(void)
{
	struct chess g, t;
	struct cmove m[CH_MAXMOVES], a;
	char buf[256], fen[CH_FEN_MAX];
	int n, i, games, ply, lv, col, reached, r;
	long t0, worst = 0;

	/* perft: well-known node counts cover castling, en passant, promotion */
	ch_new(&g);
	assert(perft(&g, 3) == 8902 && perft(&g, 4) == 197281);
	assert(!ch_set_fen(&g, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"));
	assert(perft(&g, 1) == 48 && perft(&g, 2) == 2039 && perft(&g, 3) == 97862);
	assert(!ch_set_fen(&g, "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"));
	assert(perft(&g, 4) == 43238);
	assert(!ch_set_fen(&g, "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"));
	assert(perft(&g, 3) == 9467);

	/* results */
	assert(!ch_set_fen(&g, "R5k1/5ppp/8/8/8/8/8/6K1 b - - 1 1") && ch_result(&g) == CH_MATE);
	assert(!ch_set_fen(&g, "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1") && ch_result(&g) == CH_STALEMATE);
	assert(!ch_set_fen(&g, "7k/8/6K1/8/8/8/8/5B2 b - - 0 1") && ch_result(&g) == CH_DRAW);

	/* mate in one is found */
	assert(!ch_set_fen(&g, "6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1"));
	a = ch_ai(&g, 3, 0, 0, 3000, &reached);
	assert(a.from == 56 && a.to == 0);

	/* the AI always plays a legal move; whole games end; time limit holds */
	for (games = 0; games < 3; games++) {
		ch_new(&g);
		for (ply = 0; ply < 150 && ch_result(&g) == CH_PLAYING; ply++) {
			n = ch_moves(&g, m);
			t0 = mono_ms();
			a = ch_ai(&g, games ? 64 : 1, games ? 0 : 40, games == 2 ? 0 : 300, 100, &reached);
			if (mono_ms() - t0 > worst)
				worst = mono_ms() - t0;
			for (i = 0; i < n && memcmp(&m[i], &a, sizeof a); i++)
				;
			assert(i < n);
			ch_play(&g, a);
		}
		r = ch_result(&g);
		printf("chess AI game %d: %d plies, %s\n", games, ply,
		       r == CH_MATE ? (g.turn > 0 ? "black won" : "white won") :
		       r == CH_PLAYING ? "unfinished" : "draw");
	}
	printf("chess AI: worst %ld ms (limit 100)\n", worst);

	/* saves: round trip, and damage is refused */
	ch_new(&g);
	n = ch_moves(&g, m);
	ch_play(&g, m[n - 1]);
	ch_save(&g, 4, -1, buf, sizeof buf);
	assert(!ch_load(&t, &lv, &col, buf) && lv == 4 && col == -1 && !memcmp(&g, &t, sizeof g));
	ch_fen(&t, fen);
	assert(strstr(buf, fen));
	assert(ch_load(&t, &lv, &col, "") && ch_load(&t, &lv, &col, "minios-chess 1\n"));
	strstr(buf, "level ")[6] = '9';
	assert(ch_load(&t, &lv, &col, buf));
	assert(ch_set_fen(&t, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBN w KQkq - 0 1"));  /* short rank */
	assert(ch_set_fen(&t, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w kq - 0 1"));   /* no white king */
	assert(ch_set_fen(&t, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e3 0 1")); /* bad e.p. */
	assert(ch_set_fen(&t, "4k3/8/8/8/8/8/8/4K2R w KQ - 0 1"));                        /* no a1 rook */
	assert(ch_set_fen(&t, "4k3/4Q3/8/8/8/8/8/4K3 w - - 0 1"));                        /* side not to move in check */
}

int main(void)
{
	rng_seed(12345);
	sudoku();
	checkers();
	chess();
	puts("games_test: ok");
	return 0;
}
