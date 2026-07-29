#!/usr/bin/env bash
# One-shot: apply missed x86_64-pc-msys host patch for GCC fputs_unlocked.
set -euo pipefail
CHAIN="${1:-/opt/toolchains/dc/kos/utils/dc-chain}"
cd "$CHAIN"
mkdir -p patches/x86_64-pc-msys
shopt -s nullglob
for f in patches/x86_64-pc-cygwin/*.diff; do
  base="$(basename "$f")"
  if [[ ! -f "patches/x86_64-pc-msys/$base" ]]; then
    cp -f "$f" "patches/x86_64-pc-msys/$base"
    echo "Mirrored $base -> patches/x86_64-pc-msys/"
  fi
done
for gcc_src in gcc-*/; do
  [[ -d "$gcc_src" ]] || continue
  ver="${gcc_src%/}"
  host_diff="patches/x86_64-pc-msys/${ver}.diff"
  [[ -f "$host_diff" ]] || host_diff="patches/x86_64-pc-cygwin/${ver}.diff"
  sh_cc="${gcc_src}gcc/config/sh/sh.cc"
  if [[ -f "$host_diff" && -f "$sh_cc" ]]; then
    if grep -q 'fputs() and related are redefined' "$sh_cc"; then
      echo "Already patched: $sh_cc"
    else
      echo "Applying $host_diff to $gcc_src"
      patch -N -d "$gcc_src" -p1 < "$host_diff"
    fi
  fi
done
echo "OK: host fputs patch present."
