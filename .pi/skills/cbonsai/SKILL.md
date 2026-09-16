---
name: cbonsai
description: Project context and development workflow for cbonsai, the C/ncurses bonsai tree generator. Use when modifying, building, testing, or reviewing the cbonsai repository.
---

# cbonsai

## Project shape

- Upstream: `https://gitlab.com/jallbrit/cbonsai/`
- This is a small POSIX C program using wide-character ncurses and panels.
- Most implementation lives in `cbonsai.c`: option parsing, ncurses windows, tree-growth algorithm, rendering, and save/load handling.
- `Makefile` controls compilation and installation; `cbonsai.scd` is the scdoc source for the section 6 man page; `completions/bash/cbonsai.bash` provides Bash completion.
- There is no automated test suite. Rendering is terminal-size, locale, and terminal-capability dependent.

## Build and smoke-test

From the repository root:

```bash
make                         # builds ./cbonsai
./cbonsai --help
make clean
```

Build dependencies are a C compiler, `pkg-config`, and ncursesw/panelw development libraries. `scdoc` is optional and is only needed to generate the man page. For a deterministic visual/manual check, use `./cbonsai --print --seed 1`; interactive modes require a real terminal. Install locally with `make install PREFIX="$HOME/.local"`.

## Development notes

- Keep CLI help, `README.md`, `cbonsai.scd`, and Bash completion consistent when changing options.
- The growth algorithm is intentionally tuned for the default terminal/tree size. Changes to branch geometry or Unicode rendering should be checked in both live and print modes.
- Save files contain only the seed and branch count, so option changes are not preserved across save/load.
- Avoid committing generated `cbonsai` binaries or man-page output. Preserve unrelated working-tree changes when starting work.
