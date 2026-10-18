# cbonsai (Asciiverse packaging)

John Allbritten's bonsai tree that grows in the terminal, in C on ncursesw
with panels, GPL-3.0-only; packaged for the Asciiverse catalog as a demo
(kind `art`). Not our code: keep changes small, list every one in
PATCHES.md, and offer fixes upstream. Upstream's own notes for agents are
in `.pi/skills/cbonsai/SKILL.md`.

- Remotes: `upstream` is gitlab.com/jallbrit/cbonsai (branch `master`),
  `debian` is Debian's salsa packaging repo (no patches), `origin` is
  github.com/theasciiverse/cbonsai.
- Branch `asciiverse` = upstream master plus our commits.
- Release: tag `v<version>-av.<n>` (now `v1.4.2+20260923-av.1`) and push
  only that tag; the workflow runs on `*-av.*` tags.
- av starts it as `cbonsai -S`; the save file is
  `$XDG_DATA_HOME/cbonsai/save` (`%APPDATA%\cbonsai\save` on Windows).
- Local build: `make` (pkg-config ncursesw panelw), `make test`. Static
  build: `docker run --rm -v "$PWD":/src -w /src alpine:3.22 sh -c 'apk add
  build-base ncurses-dev ncurses-static && LIBS="-lpanelw -lncursesw"
  LDFLAGS="-static -s" asciiverse/build.sh'`, best in a copy of the tree,
  since the container writes root-owned files.
- Man page: `asciiverse/cbonsai.6` is generated from `cbonsai.scd`; rerun
  scdoc when that changes.
- Preview: `asciiverse/record-preview.sh out/cbonsai asciiverse/cbonsai.cast`.
