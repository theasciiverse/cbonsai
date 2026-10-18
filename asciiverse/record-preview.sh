#!/bin/sh
# Records the catalog preview at 100x30 the way av starts cbonsai (-S, live
# growth, a new tree every few seconds), with no save file yet, so the first
# tree grows from the start. SIGTERM after the given seconds quits cbonsai the
# way q does. Needs asciinema 3.
#
#   asciiverse/record-preview.sh out/cbonsai asciiverse/cbonsai.cast [seed] [seconds]
set -eu
binary=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
out=$2
seed=${3:-0}
seconds=${4:-50}
home=$(mktemp -d)
trap 'rm -rf "$home"' EXIT
mkdir "$home/bin" && cp "$binary" "$home/bin/cbonsai"
seedarg=
[ "$seed" = 0 ] || seedarg="-s $seed"
cd "$home"  # nothing in the recording names a local path
HOME="$home" XDG_DATA_HOME= PATH="$home/bin:$PATH" TERM=xterm-256color COLORTERM=truecolor LANG=C.UTF-8 \
asciinema rec --headless --overwrite -q \
  --window-size 100x30 --idle-time-limit 2 \
  --capture-env "TERM,COLORTERM,LANG" --title "cbonsai" \
  --command "timeout --foreground -s TERM $seconds cbonsai -S $seedarg" \
  "$OLDPWD/$out"
