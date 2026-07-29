#!/usr/bin/env bash
# Clean stale dc-chain build dirs that commonly break resumes on MSYS2.
set -euo pipefail
CHAIN="${1:-/opt/toolchains/dc/kos/utils/dc-chain}"
cd "$CHAIN"

# Stop leftover builders from a previous install/resume
pkill -f 'dc-chain.*(make|gcc|cc1)' 2>/dev/null || true
pkill -f 'build-binutils-sh-elf' 2>/dev/null || true
sleep 1

# Corrupted by parallel configure / overlapping installs:
#   Makefile: .deps/elf32-sparc.Plo: No such file or directory
if [[ -d build-binutils-sh-elf-2.43 ]]; then
  echo "Removing stale build-binutils-sh-elf-2.43"
  rm -rf build-binutils-sh-elf-2.43
fi
# Same class of issue can hit other binutils builds
for d in build-binutils-*; do
  [[ -d "$d" ]] || continue
  if [[ -f "$d/bfd/Makefile" ]] && grep -q 'elf32-sparc\.Plo' "$d/bfd/Makefile" 2>/dev/null; then
    if [[ ! -f "$d/bfd/.deps/elf32-sparc.Plo" ]]; then
      echo "Removing corrupted $d (stale BFD deps)"
      rm -rf "$d"
    fi
  fi
done

echo "OK: binutils build dirs cleaned."
