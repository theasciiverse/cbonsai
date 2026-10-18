#!/bin/sh
# Builds cbonsai into out/. The compiler and curses libraries come from the
# environment, so one script serves every target (see
# .github/workflows/release.yml):
#
#   asciiverse/build.sh                                          # Linux
#   LIBS="-lpanelw -lncursesw" LDFLAGS="-static -s" asciiverse/build.sh   # Alpine, static
#   CFLAGS="-O2 -D_XOPEN_SOURCE_EXTENDED" LIBS="-lpanel -lncurses" asciiverse/build.sh   # macOS
#   CC=x86_64-w64-mingw32-gcc CFLAGS="-O2 -I$PDC -DPDC_WIDE -DPDC_FORCE_UTF8" \
#     LIBS="$PDC/wincon/pdcurses.a -lpthread" LDFLAGS="-static -s" EXE=.exe asciiverse/build.sh
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--O2}"
LDFLAGS="${LDFLAGS:-}"
LIBS="${LIBS:-$(pkg-config --libs ncursesw panelw 2>/dev/null || echo -lpanelw -lncursesw)}"
EXE="${EXE:-}"
mkdir -p out
$CC $CFLAGS -Wall -Wextra cbonsai.c $LDFLAGS $LIBS -o "out/cbonsai$EXE"
echo "built cbonsai$EXE"
