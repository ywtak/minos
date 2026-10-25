/* minios: every layout, timing and device number lives here.
 * Values marked VERIFY come from community docs, not this unit; confirm
 * them with `minios keys` / scripts/discover.sh and record in NOTES.md. */
#ifndef CONFIG_H
#define CONFIG_H

/* screen */
#define SCREEN_W            600
#define SCREEN_H            800
#define FB_INVERT           1      /* eink_fb: 0x00 = white (verified on device) */

/* home screen */
#define HEADER_TEXT         "All books"
#define HEADER_H            72
#define HEADER_PX           36
#define GRID_COLS           3
#define GRID_ROWS           3
#define GRID_MARGIN         18     /* left/right/bottom page margin */
#define GRID_GAP            14     /* space between cells */
#define BAND_H              46     /* black title band at bottom of cover */
#define BAND_PX             16     /* title font size */
#define BAND_PAD            6
#define SEL_BORDER          5      /* selection frame thickness */
#define FOOTER_H            28     /* "1 / 3" under the grid */
#define FOOTER_PX           16
#define FONT_NAME           "Helvetica"

/* reader */
#define READER_EM           22     /* EPUB body font size, px */
#define READER_MARGIN       24     /* px around reflowed EPUB text */
#define PDF_MARGIN          6      /* px around a PDF page cropped to its content */
#define PDF_OVERLAP         40     /* px repeated between pieces of a tall PDF page */
#define PDF_SQUEEZE         10     /* % too tall that is shrunk to fit instead of split */
#define FULL_REFRESH_EVERY  6      /* page changes between flashing refreshes (PDF halves do not count) */

/* overlay menus (Menu key) */
#define MENU_W              360
#define MENU_ROW            56     /* height of the title and of each entry */
#define MENU_PX             24
#define MENU_BORDER         3

/* screensaver */
#define SAVER_IDLE_MS       60000  /* no key for this long: screensaver */
#define SAVER_FILES         { "screensaver.png", "screensaver.jpg", "screensaver.jpeg" }   /* drive root */

/* timing */
#define POLL_MS             1000   /* how often to check the USB cable */
#define KEY_REPEAT_MS       180    /* ignore repeats faster than this */

/* e-ink: Lab126 eink_fb shim owns /dev/fb0 (linux/einkfb.h) */
#define FBIO_EINK_UPDATE_DISPLAY 0x46db
#define FX_UPDATE_PARTIAL   0
#define FX_UPDATE_FULL      1
#define FX_UPDATE_FAST      2      /* 1-bit: the driver posterizes the area first */
#define FBIO_EINK_UPDATE_DISPLAY_AREA 0x46dd

/* raw evdev key codes, verified on device with `minios keys` */
#define K_UP                103
#define K_DOWN              108
#define K_LEFT              105
#define K_RIGHT             106
#define K_SELECT            194    /* 5-way centre */
#define K_NEXT              191    /* page forward (both sides) */
#define K_PREV              193    /* page back */
#define K_NEXT2             104    /* left-side next page (verified on device) */
#define K_PREV2             109    /* right-side previous page (verified on device) */
#define K_HOME              102
#define K_BACK              158
#define K_MENU              139
#define K_POWER             116

/* paths */
#ifdef HOST
#define USERSTORE_MNT       "us"
#else
#define USERSTORE_MNT       "/mnt/us"
#endif
#define INPUT_DIR           "/dev/input"
#define USERSTORE_DEV       "/dev/mmcblk0p4"   /* exported whole to the PC (verified) */
#define USERSTORE_LOOP      "/dev/loop0"       /* the FAT starts 8 KiB into p4 (verified) */
#define USERSTORE_ATTACH    "losetup -o 8192 " USERSTORE_LOOP " " USERSTORE_DEV
#define USERSTORE_DETACH    "losetup -d " USERSTORE_LOOP
#define BOOKS_DIR           USERSTORE_MNT "/books"
#define STATE_DIR           USERSTORE_MNT "/.minios"
#define POS_FILE            STATE_DIR "/pos"
#define COVER_DIR           STATE_DIR "/covers"
#define MAX_BOOKS           512
#define SCAN_DEPTH          4      /* folder levels searched for books */

/* USB cable: first readable file starting with '1' means connected (verified) */
#define USB_ONLINE_PATHS    { "/sys/devices/platform/fsl-usb2-udc/connected", 0 }
#define USB_EXPORT_CMD      "insmod /lib/modules/g_file_storage.ko file=" USERSTORE_DEV " removable=1 stall=0"
#define USB_UNEXPORT_CMD    "rmmod g_file_storage"

#endif
