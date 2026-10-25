/* make test: Sudoku generator/saves and Checkers rules/AI/saves. */
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

int main(void)
{
	rng_seed(12345);
	sudoku();
	checkers();
	puts("games_test: ok");
	return 0;
}
