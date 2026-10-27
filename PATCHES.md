# Changes from upstream

This is the Asciiverse packaging of [cbonsai](https://gitlab.com/jallbrit/cbonsai)
by John Allbritten and contributors, a bonsai tree that grows in the
terminal (C, ncursesw with panels). Upstream is the `upstream` remote; the
`asciiverse` branch carries the changes below.

Built from upstream `master` at `bca4e74` (2026-09-23), not the v1.4.2 tag
(2025-06-19): the 17 commits since add tests, text placement with `-T` and
`-g`, safer message drawing, and trees that are redrawn instead of lost when
the terminal is resized. That last one matters for a screensaver. Upstream
has tagged nothing after v1.4.2, so the version here is 1.4.2+20260923.

## Licence

GPL-3.0-only. Upstream ships the GPL version 3 text as LICENSE and no notice
in the source or README offers "any later version"; Debian's copyright file
reads "version 3 of the License" (GPL-3). With nothing granting later
versions, this packaging stays on version 3 only.

## Debian

Debian packages 1.4.2-1 (Robin Gustafsson) with no patches: its `debian/`
has no `patches/` folder, only build rules that pass the hardening flags.
Its packaging repository (`Vcs-Git`) is the `debian` remote,
https://salsa.debian.org/rgson/cbonsai.git, branch `debian/latest`.

## Code

- **Infinite mode remembers each new tree's seed.** After the first tree,
  infinite mode reseeded the random numbers but kept the first tree's seed
  in `conf.seed`. A resize replays the current tree from that seed, so
  resizing during the second or any later tree drew the first tree again
  (checked in tmux before and after), and quitting saved the first tree's
  seed with the current tree's branch count. Worth offering upstream.
- **The save file lives in the data folder.** The default file for
  `--save`, `--load` and screensaver mode moved from
  `$XDG_CACHE_HOME/cbonsai` (`~/.cache/cbonsai`) to
  `$XDG_DATA_HOME/cbonsai/save` (`~/.local/share/cbonsai/save`), or
  `%APPDATA%\cbonsai\save` on Windows; its folder is made when saving. A
  default save file that does not exist yet (the first run of screensaver
  mode) is no longer reported as an error: the message was printed before
  the screen opened and appeared after quitting. Help, man page and README
  say the same.
- **Ctrl+C and SIGTERM quit the way q does.** Ctrl+C used to kill cbonsai
  by its signal (exit 130), so screensaver mode never saved the tree it was
  growing, though the README says quitting saves it. The signals now set a
  flag the key check reads: the screen closes, the save is written, exit
  status 0. Worth offering upstream.
- **Windows build with MinGW and PDCurses**, all behind `_WIN32` or
  `PDCURSES`: a `wcwidth()` (the Windows C library has none) after Markus
  Kuhn's public-domain one, with the main combining ranges; `resizeterm()`
  mapped to PDCurses' `resize_term()`; no `SIGWINCH` handler; `-T` reopens
  the console as `CONIN$` instead of `ctermid()`; folders made with
  `_mkdir()`. `nanosleep()` comes from winpthreads (`-lpthread`).

## Packaging

- `asciiverse/build.sh` and `asciiverse/package.sh` build and pack one
  target; the compiler and curses libraries come from the environment.
- `.github/workflows/release.yml`: av archives for Linux (static musl,
  x86-64 and ARM64, upstream's tests run on the static build), macOS
  (x86-64 and ARM64, system ncurses) and Windows (x86-64, wide PDCurses
  3.9), plus SHA256SUMS. It runs on our tags only (`*-av.*`).
- `asciiverse/cbonsai.6`: the man page, generated from `cbonsai.scd` with
  scdoc 1.11.3 (`scdoc < cbonsai.scd > asciiverse/cbonsai.6`), so the
  release builds need no scdoc. Regenerate it when `cbonsai.scd` changes.
- `asciiverse/record-preview.sh` records `asciiverse/cbonsai.cast`, the
  catalog preview: 50 seconds of `cbonsai -S` at 100x30 with no save file,
  three trees growing, ended by SIGTERM.

av starts cbonsai as `cbonsai -S`: live growth, a new tree every 4 seconds,
any key quits, and the tree in progress is saved and grown on next time.

## Checked

- Linux, native and static (Alpine 3.22) builds: upstream's tests pass;
  screensaver mode at 100x30 under TERM tmux-256color, xterm-256color,
  screen and linux; quitting with a key, q and Ctrl+C in every mode; the
  save written to and resumed from `~/.local/share/cbonsai/save` and
  `$XDG_DATA_HOME/cbonsai/save`; an ASan and UBSan build grown and resized
  at 80x24, 60x20, 40x12, 31x8, 20x6 and 10x4, clean.
- Windows: cross-built with MinGW against PDCurses 3.9 (wincon, wide) and
  run under Wine 10: screensaver mode grows and quits on a key with the
  save in `%APPDATA%\cbonsai\save`, q quits infinite mode, a message box
  draws. A resize under Wine did not reach the program (Wine's console in
  tmux keeps its size), so resizing on Windows is unchecked.
- macOS: not built here.

## Ours

- **How to play it, at the top of README.md.** A short note on installing and
  starting it with av, or from a release archive; the original text follows
  unchanged.
