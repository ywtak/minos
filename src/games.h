/* Game rules without any drawing: sudoku.c and checkers.c, used by
 * games.c and tests/games_test.c. */
#ifndef GAMES_H
#define GAMES_H
#include <stdint.h>

/* sudoku.c, shared by both games */
void rng_seed(uint32_t s);
uint32_t rng(void);
long mono_ms(void);

/* sudoku.c */
struct sudoku {
	uint8_t grid[81];                          /* 0 = empty */
	uint8_t given[81];
	uint8_t sol[81];
	uint8_t wrong[81];                         /* marked by Check */
	int level, secs, show_conflicts;
};
int sdk_count(const uint8_t *g, int limit);     /* solutions of g, counted up to limit */
int sdk_new(struct sudoku *s, int level);       /* returns the number of clues */
int sdk_conflict(const struct sudoku *s, int i);/* digit at i repeats in its row/column/box */
int sdk_solved(const struct sudoku *s);
void sdk_save(const struct sudoku *s, char *buf, int n);
int sdk_load(struct sudoku *s, const char *text);   /* 0 = ok; anything odd is refused */

/* checkers.c: English draughts, white at the bottom moves first */
#define CK_MAXSTEPS 64
struct ck {
	int8_t b[64];                              /* +1/+2 white man/king, -1/-2 black, 0 empty */
	int turn;                                  /* +1 white, -1 black */
	int chain;                                 /* square of a piece that must jump on, or -1 */
};
struct step { int8_t from, to, cap; };          /* cap = captured square, or -1 */
void ck_new(struct ck *g);
int ck_steps(const struct ck *g, struct step *out); /* legal steps; 0 = side to move lost */
int ck_play(struct ck *g, struct step s);       /* 1 = the same piece must jump again */
struct step ck_ai(const struct ck *g, int depth, int random_pct, long ms, int *reached);
void ck_save(const struct ck *g, int level, char *buf, int n);
int ck_load(struct ck *g, int *level, const char *text);  /* 0 = ok */

#endif
