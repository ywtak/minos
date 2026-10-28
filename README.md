# minios

A tiny replacement operating system for the **Kindle 5th generation (2012, non-touch, "Kindle 4")** on firmware 4.1.x.
It boots straight into a grid of your books and does nothing else. There's no store, no sync, and no ads.

minios is one static program that runs as PID 1 on Amazon's own kernel. It reads EPUB and PDF with [MuPDF](https://mupdf.com). All state (reading positions, cover cache, saves, log) lives on the USB drive under `.minios/`.

## Versions

The repository is built in layers. Each layer is a git tag with a matching [release](https://github.com/ywtak/minos/releases):

| Tag | What you get |
|---|---|
| [`v1.0-bare`](https://github.com/ywtak/minos/tree/v1.0-bare) | **Base.** Library grid with covers, EPUB/PDF reader (positions saved on every page), USB drive mode for copying books, a screensaver after one idle minute (`screensaver.png`/`.jpg` on the drive root), and power off. |
| [`v1.1-battery`](https://github.com/ywtak/minos/tree/v1.1-battery) | Bare + **battery update.** Battery icon and percentage in the header. Press up from the top row to select it, then centre to see the time left at the current drain. |
| [`v1.2-games`](https://github.com/ywtak/minos/tree/v1.2-games) | Battery + **games update.** Press Menu on the library to open Sudoku (Easy/Medium/Hard) or Checkers (vs the Kindle at three levels, or two players). Games are saved on the drive. |
| [`v1.3-chess`](https://github.com/ywtak/minos/tree/v1.3-chess) | Games + **chess and sleep update.** Chess against the Kindle, its strength picked by Elo (400-2000, approximate). The screensaver now suspends the Kindle like the stock one (press power to wake), instead of keeping the CPU awake (not yet measured on hardware). |

`main` is always the latest full version. To build a specific one, run `git checkout v1.0-bare` (or another tag) before `make release`. You can also download that tag's source zip from its release page.

To fork minios, start from `v1.0-bare`: it's the smallest version that still works.

## Installing

**[INSTALL.md](INSTALL.md)** walks you through everything step by step, starting from a stock Kindle that has never been jailbroken: jailbreak, SSH, a full backup, building, installing, updating, and going back to stock.

The releases contain no prebuilt images. A minios system image includes Amazon's kernel modules, which the build copies out of **your own** Kindle's backup, so everyone builds their own.

## Keys

| Where | Key | Does |
|---|---|---|
| Library | 5-way | move the selection |
| | page keys | previous/next page of books |
| | centre | open the book (or the battery box, if selected) |
| | Menu | Games: Sudoku, Checkers (v1.2), Chess (v1.3) |
| | Power | power off; press again to start |
| Book | page keys / 5-way | turn pages |
| | Home / Back / Menu | back to the library |

## Repository layout

| Path | What |
|---|---|
| `src/` | minios itself (`main.c` = init + UI loop, `library.c`, `reader.c`, `gfx.c`, games in `games.c`/`sudoku.c`/`checkers.c`/`chess.c`). `host.c` is a PC backend for testing without a Kindle. |
| `config.h` | every layout value, timing and device number |
| `rootfs/` | `/etc` of the minios system (module list, `rc.local`, mdev) |
| `scripts/` | `flash.sh` (runs on the Kindle: install/restore), `discover.sh` (read-only device survey), `layout.env` (eMMC offsets), rootfs and kernel build helpers |
| `kernel/` | headers missing from Amazon's 4.1.4 GPL tarball, for the optional `make kernel` |
| `NOTES.md` | every hardware fact minios relies on, and how sure we are of it |
| `CHANGELOG.md` | what changed in each version |

## Status and warning

minios is a hobby project tested on one Kindle 5th gen (serial prefix `9023`, firmware 4.1.4). Installing it replaces the Kindle's system partition. You can go back with your backup (INSTALL.md, step 9), but **you do this at your own risk**.

## License

GPL-2.0 (see `LICENSE`). The headers in `kernel/` come from Amazon's GPL kernel source.
