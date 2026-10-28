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

/* chess.c: white at the bottom moves first; squares 0 = a8 .. 63 = h1 */
#define CH_MAXMOVES 256
#define CH_FEN_MAX  96
enum { CH_PAWN = 1, CH_KNIGHT, CH_BISHOP, CH_ROOK, CH_QUEEN, CH_KING };
enum { CH_PLAYING, CH_MATE, CH_STALEMATE, CH_DRAW };   /* MATE: the side to move lost */
struct chess {
	int8_t b[64];                              /* +white / -black CH_PAWN..CH_KING, 0 empty */
	int turn;                                  /* +1 white, -1 black */
	int castle;                                /* rights K Q k q = 1 2 4 8 */
	int ep;                                    /* en passant target square, or -1 */
	int half, full;                            /* 50-move clock, move number */
};
struct cmove { int8_t from, to, promo; };       /* promo = CH_KNIGHT..CH_QUEEN, or 0 */
void ch_new(struct chess *g);
int ch_moves(const struct chess *g, struct cmove *out);   /* legal moves */
void ch_play(struct chess *g, struct cmove m);
int ch_in_check(const struct chess *g);
int ch_result(const struct chess *g);
struct cmove ch_ai(const struct chess *g, int depth, int random_pct, int noise, long ms, int *reached);
void ch_fen(const struct chess *g, char *out);  /* out: CH_FEN_MAX bytes */
int ch_set_fen(struct chess *g, const char *fen);         /* 0 = ok */
void ch_save(const struct chess *g, int level, int color, char *buf, int n);
int ch_load(struct chess *g, int *level, int *color, const char *text);   /* 0 = ok */

#endif
