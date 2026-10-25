/* Reading positions: one "book<TAB>chapter<TAB>page" line per book.
 * Rewritten through a temp file + rename so a power cut never leaves
 * a half-written file. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int is_book(const char *line, const char *book, size_t n)
{
	return !strncmp(line, book, n) && line[n] == '\t';
}

int pos_get(const char *file, const char *book, int *chap, int *page)
{
	char line[512];
	size_t n = strlen(book);
	int found = 0;
	FILE *f = fopen(file, "r");

	if (!f)
		return 0;
	while (fgets(line, sizeof line, f))
		if (is_book(line, book, n) && sscanf(line + n + 1, "%d\t%d", chap, page) == 2)
			found = 1;
	fclose(f);
	return found;
}

/* Replace file with text the same way: temp file, fsync, rename. */
int file_put(const char *file, const char *text)
{
	char tmp[512];
	FILE *out;

	snprintf(tmp, sizeof tmp, "%s.tmp", file);
	if (!(out = fopen(tmp, "w")))
		return -1;
	fputs(text, out);
	fflush(out);
	fsync(fileno(out));
	if (fclose(out))
		return -1;
	return rename(tmp, file);
}

int pos_put(const char *file, const char *book, int chap, int page)
{
	char tmp[512], line[512];
	size_t n = strlen(book);
	FILE *in, *out;

	snprintf(tmp, sizeof tmp, "%s.tmp", file);
	if (!(out = fopen(tmp, "w")))
		return -1;
	if ((in = fopen(file, "r"))) {
		while (fgets(line, sizeof line, in))
			if (!is_book(line, book, n))
				fputs(line, out);
		fclose(in);
	}
	fprintf(out, "%s\t%d\t%d\n", book, chap, page);
	fflush(out);
	fsync(fileno(out));
	if (fclose(out))
		return -1;
	return rename(tmp, file);
}
