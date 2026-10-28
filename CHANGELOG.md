# Changelog

Every minios version, newest first. Each one is a git tag with a matching
[release](https://github.com/ywtak/minos/releases) and builds on the one below it.

## Unreleased

### Docs
- New README:
  - Screenshots of every screen, made with the PC build, in `docs/screenshots/`.
  - A seven-step install overview that links into INSTALL.md.
  - A section on other devices: what would port, and how.
  - A risks table.
- INSTALL.md now builds `v1.3-chess` by default, explains waking from sleep and mentions Chess, and links to the risks.

## v1.3-chess — 2026-10-28

### Added
- **Chess** (Menu → Games → Chess) against the Kindle. Pick its strength by Elo: 400, 600, 800, 1000, 1200 (default), 1400, 1600, 1800 or 2000.
  - The Elo numbers are approximate. Lower levels search less deeply, sometimes play a random move and judge positions less precisely. The table is `CHESS_*` in `config.h`.
  - The Kindle thinks for at most 3 s a move (`CHESS_AI_MS`). The search depth and time of each move go to `.minios/log.txt`.
  - All the rules: castling, en passant, promotion, check, checkmate, stalemate, the 50-move rule and insufficient material.
  - Play as White or Black (the board flips), undo, and dots on the legal targets. The Kindle's last move is marked.
  - Saved after every move to `.minios/chess`.
  - Pieces are drawn with the chess symbols built into MuPDF, so no font files are needed.
- `CHANGELOG.md` (this file).

### Changed
- **Idle power.** The screensaver now suspends the Kindle to RAM, as the stock firmware does, instead of waking the CPU every second. Press power to wake it.
  - If the kernel refuses to suspend, minios logs why and falls back to the old behaviour.
  - Look for `suspend: resumed` or the error in `.minios/log.txt`.
- At boot, `rc.local` sets the CPU clock to scale down when idle (cpufreq `ondemand` and i.MX DVFS), when the kernel supports it.

### Known limits
- Pawns always promote to a queen.
- Three repeated positions are not a draw. The 50-move rule still ends endless games.
- Suspend is not yet verified on hardware: see `NOTES.md`, "USB and power".
- Plugging in USB may not wake the Kindle by itself. Press power, and USB mode starts.

## v1.2-games — 2026-10-25

### Added
- Menu on the library opens Games.
- **Sudoku**: Easy, Medium or Hard. Puzzles are generated with a unique solution. Includes a timer, and the game is saved on the drive.
- **Checkers**: against the Kindle (Easy, Normal or Hard) or two players. Includes undo, and the game is saved on the drive.

## v1.1-battery — 2026-10-25

### Added
- Battery icon and percentage in the library header. Press up from the top row to select it, then centre to see the time left at the current drain.

## v1.0-bare — 2026-10-25

### Added
- Library grid with covers.
- EPUB/PDF reader. Your position is saved on every page.
- USB drive mode for copying books.
- Screensaver after one idle minute, using `screensaver.png` or `.jpg` from the drive root.
- Power off.
