#!/bin/sh
# Translate common wget flags to curl (MSYS wget is broken here).
set -e
out=""
args=""
while [ $# -gt 0 ]; do
  case "$1" in
    -O|--output-document=*)
      if [ "$1" = "-O" ]; then
        shift
        out=$1
      else
        out=${1#--output-document=}
      fi
      ;;
    -o)
      shift
      out=$1
      ;;
    --no-verbose|-q|--quiet)
      args="$args -sS"
      ;;
    -c|--continue)
      args="$args -C -"
      ;;
    -*)
      # ignore unknown wget flags
      ;;
    *)
      url=$1
      ;;
  esac
  shift
done
if [ -z "${url:-}" ]; then
  echo "wget-shim: missing URL" >&2
  exit 1
fi
if [ -n "$out" ]; then
  exec curl -L --fail $args -o "$out" "$url"
else
  exec curl -L --fail $args -O "$url"
fi
