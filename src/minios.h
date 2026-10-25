#ifndef MINIOS_H
#define MINIOS_H
#include <stdint.h>
#include <mupdf/fitz.h>
#include "../config.h"

enum key { KNONE, KUP, KDOWN, KLEFT, KRIGHT, KOK, KNEXT, KPREV, KHOME, KPOWER, KMENU, KBACK };

extern fz_context *ctx;
extern uint8_t screen[SCREEN_W * SCREEN_H];   /* 8-bit gray, 0 = black */

/* main.c */
void logmsg(const char *fmt, ...);
int input_next(int timeout_ms);                /* enum key, KNONE on timeout; runs the screensaver */
extern long saver_ms;                          /* total time spent in the screensaver */

/* backend: eink.c + input.c + usb.c on device, host.c on PC */
int disp_init(void);
void disp_flush(int full);
void disp_area(int x, int y, int w, int h);    /* fast 1-bit refresh of one rectangle */
int input_init(void);
int input_raw(int timeout_ms);                 /* raw key code, -1 on timeout */
int usb_online(void);
void usb_export(void);
void usb_unexport(void);

/* gfx.c */
void gfx_init(void);
void gfx_fill(int x, int y, int w, int h, int gray);
int gfx_text_w(const char *s, int px);
void gfx_text(int x, int baseline, int px, int gray, const char *s);
void gfx_text_fit(int x, int baseline, int px, int gray, const char *s, int maxw);
void gfx_blit(const uint8_t *src, int w, int h, int stride, int x, int y);
void gfx_message(const char *s);
void gfx_text_bold(int x, int baseline, int px, int gray, const char *s);
void gfx_frame(int x, int y, int w, int h, int t, int gray);
void gfx_disc(int cx, int cy, int r, int gray);
void gfx_dither(int x, int y, int w, int h);  /* light gray from black/white pixels */
int key_wait(void);                            /* next key; KNONE = USB cable plugged in */
/* Boxed list over the current screen. Returns the chosen entry, or -1 with
 * *key = KBACK/KMENU (closed), KHOME, KPOWER or KNONE (USB). */
int menu_run(const char *title, const char **items, int n, int sel, int *key);

/* pos.c */
int pos_get(const char *file, const char *book, int *chap, int *page);
int pos_put(const char *file, const char *book, int chap, int page);
int file_put(const char *file, const char *text);   /* atomic replace */

/* library.c */
struct book { char name[256]; char title[160]; };
int lib_scan(struct book *b, int max);
void lib_draw(struct book *b, int n, int sel);

/* reader.c */
int reader_run(const char *name);              /* closing key; KNONE = USB plugged */

#endif
