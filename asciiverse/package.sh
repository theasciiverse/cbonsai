#!/bin/sh
# Packs the build into dist/cbonsai-<target>.tar.gz (.zip for Windows) for
# av: the binary at the top beside its man page, README, LICENSE and
# PATCHES.md.
#
#   asciiverse/package.sh x86_64-unknown-linux-musl
set -eu
cd "$(dirname "$0")/.."
target="$1"
case "$target" in *windows*) exe=.exe ;; *) exe= ;; esac
stage=stage/cbonsai
rm -rf "$stage" && mkdir -p "$stage" dist
cp "out/cbonsai$exe" asciiverse/cbonsai.6 README.md LICENSE PATCHES.md "$stage/"
if [ -n "$exe" ]; then
  (cd "$stage" && zip -qr "../../dist/cbonsai-$target.zip" .)
else
  tar -czf "dist/cbonsai-$target.tar.gz" -C "$stage" .
fi
ls dist
