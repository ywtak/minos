/* make test: round-trip and overwrite checks for src/pos.c */
#include <assert.h>
#include <stdio.h>

int pos_get(const char *file, const char *book, int *chap, int *page);
int pos_put(const char *file, const char *book, int chap, int page);

int main(void)
{
	const char *f = "pos_test.tmp";
	int c = -1, p = -1;

	remove(f);
	assert(!pos_get(f, "a.epub", &c, &p));
	assert(pos_put(f, "a.epub", 2, 14) == 0);
	assert(pos_put(f, "b.pdf", 0, 99) == 0);
	assert(pos_put(f, "a.epub", 3, 1) == 0);          /* overwrite, not append */
	assert(pos_get(f, "a.epub", &c, &p) && c == 3 && p == 1);
	assert(pos_get(f, "b.pdf", &c, &p) && c == 0 && p == 99);
	assert(!pos_get(f, "a.ep", &c, &p));                /* prefix is not a match */
	remove(f);
	puts("pos_test: ok");
	return 0;
}
