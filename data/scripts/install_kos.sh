#!/usr/bin/env bash
# Install KallistiOS + dc-chain under MSYS2 for Pyrite64 Dreamcast builds.
# Invoked by install_kos.bat
set -euo pipefail

SKIP_TOOLCHAIN=0
FORCE=0
for arg in "$@"; do
  case "$arg" in
    skip-toolchain|SKIP_TOOLCHAIN=1) SKIP_TOOLCHAIN=1 ;;
    force|FORCE=1) FORCE=1 ;;
  esac
done

DC_ROOT="/opt/toolchains/dc"
KOS_BASE="${KOS_BASE:-$DC_ROOT/kos}"
KOS_PORTS="$DC_ROOT/kos-ports"
MKDCDISC_DIR="$DC_ROOT/mkdcdisc"
KOS_BRANCH="v2.2.x"

echo "==> Target KOS_BASE=$KOS_BASE"
echo "==> Branch $KOS_BRANCH"
echo

# ---------------------------------------------------------------------------
# Dependencies (MSYS2)
# ---------------------------------------------------------------------------
echo "[1/6] Fixing MSYS2 pacman keys / sync DBs (if needed)..."
# Stale lock from a crashed/interrupted pacman run
if [[ -f /var/lib/pacman/db.lck ]]; then
  echo "    Removing stale pacman lock /var/lib/pacman/db.lck"
  rm -f /var/lib/pacman/db.lck
fi

if [[ ! -d /etc/pacman.d/gnupg ]] || ! pacman-key --list-keys >/dev/null 2>&1; then
  rm -rf /etc/pacman.d/gnupg
  pacman-key --init
  pacman-key --populate msys2
else
  pacman-key --populate msys2 || true
fi

# Refresh keyring; if sync fails, bootstrap keyring package from the web
if ! pacman -Sy --noconfirm msys2-keyring 2>/dev/null; then
  echo "    Trying keyring bootstrap from repo.msys2.org..."
  rm -f /var/lib/pacman/db.lck
  tmpdir="$(mktemp -d)"
  (
    cd "$tmpdir"
    curl -fsSL -o keyring.pkg.tar.zst \
      "https://repo.msys2.org/msys/x86_64/msys2-keyring-1~20260214-1-any.pkg.tar.zst" \
      || curl -fsSL -o keyring.pkg.tar.zst \
      "https://repo.msys2.org/msys/x86_64/msys2-keyring-1~20251012-1-any.pkg.tar.zst" \
      || true
    if [[ -f keyring.pkg.tar.zst ]]; then
      pacman -U --noconfirm keyring.pkg.tar.zst || pacman -U --noconfirm --overwrite='*' keyring.pkg.tar.zst
      pacman-key --populate msys2 || true
    fi
  )
  rm -rf "$tmpdir"
fi

echo "    Syncing package databases..."
rm -f /var/lib/pacman/db.lck
if ! pacman -Sy --noconfirm; then
  echo
  echo "ERROR: pacman sync failed."
  echo "If you see db.lck errors, close other MSYS2/pacman windows and run:"
  echo "  rm -f /var/lib/pacman/db.lck"
  echo "  pacman -Syu"
  echo "Then re-run install_kos.bat"
  exit 1
fi

echo "Installing build dependencies via pacman..."
# Host utils (dcbumpgen/kmgenc/vqenc) need libpng + libjpeg headers/libs.
# Prefer UCRT64 MinGW packages; build those tools with /ucrt64/bin/gcc.
pacman -S --needed --noconfirm \
  git make wget tar patch diffutils texinfo bison flex gawk curl \
  libnettle libgnutls \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-meson \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-libpng \
  mingw-w64-ucrt-x86_64-libjpeg-turbo \
  mingw-w64-ucrt-x86_64-zlib \
  python3 \
  || pacman -S --needed --noconfirm \
  git make wget tar patch diffutils texinfo bison flex gawk curl \
  libnettle libgnutls \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-meson \
  mingw-w64-x86_64-pkgconf \
  mingw-w64-x86_64-libpng \
  mingw-w64-x86_64-libjpeg-turbo \
  mingw-w64-x86_64-zlib \
  python3

# GMP/MPFR/MPC headers used by dc-chain (prefer mingw, fall back to msys)
pacman -S --needed --noconfirm \
  mingw-w64-ucrt-x86_64-gmp mingw-w64-ucrt-x86_64-mpfr mingw-w64-ucrt-x86_64-mpc \
  2>/dev/null || pacman -S --needed --noconfirm \
  mingw-w64-x86_64-gmp mingw-w64-x86_64-mpfr mingw-w64-x86_64-mpc \
  2>/dev/null || pacman -S --needed --noconfirm gmp-devel mpfr mpc 2>/dev/null || true

mkdir -p "$DC_ROOT"

# ---------------------------------------------------------------------------
# Clone / update KallistiOS
# ---------------------------------------------------------------------------
echo
echo "[2/6] Cloning KallistiOS ($KOS_BRANCH)..."
if [[ -d "$KOS_BASE/.git" ]]; then
  git -C "$KOS_BASE" fetch --tags origin
  git -C "$KOS_BASE" checkout "$KOS_BRANCH"
  git -C "$KOS_BASE" pull --ff-only origin "$KOS_BRANCH" || true
else
  if [[ -d "$KOS_BASE" && "$FORCE" == "1" ]]; then
    rm -rf "$KOS_BASE"
  fi
  if [[ ! -d "$KOS_BASE" ]]; then
    git clone --branch "$KOS_BRANCH" --depth 1 https://github.com/KallistiOS/KallistiOS.git "$KOS_BASE"
  fi
fi

# ---------------------------------------------------------------------------
# dc-chain (SH-4 / ARM toolchains) — long step
# ---------------------------------------------------------------------------
echo
if [[ "$SKIP_TOOLCHAIN" == "1" ]]; then
  echo "[3/6] Skipping dc-chain (skip-toolchain)."
else
  if command -v sh-elf-gcc >/dev/null 2>&1 && [[ "$FORCE" != "1" ]]; then
    echo "[3/6] sh-elf-gcc already on PATH — skipping dc-chain. Pass 'force' to rebuild."
  elif [[ -x "/opt/toolchains/dc/sh-elf/bin/sh-elf-gcc" || -x "/opt/toolchains/dc/sh-elf/bin/sh-elf-gcc.exe" ]] && [[ "$FORCE" != "1" ]]; then
    echo "[3/6] Found existing sh-elf toolchain — skipping dc-chain. Pass 'force' to rebuild."
  else
    echo "[3/6] Building dc-chain (this is the long step)..."
    CHAIN_DIR="$KOS_BASE/utils/dc-chain"
    if [[ ! -d "$CHAIN_DIR" ]]; then
      # Newer KOS master uses kos-chain
      CHAIN_DIR="$KOS_BASE/utils/kos-chain"
    fi
    if [[ ! -d "$CHAIN_DIR" ]]; then
      echo "ERROR: dc-chain directory not found under $KOS_BASE/utils"
      exit 1
    fi
    cd "$CHAIN_DIR"
    if [[ -f Makefile.default.cfg ]]; then
      cp -n Makefile.default.cfg Makefile.cfg 2>/dev/null || cp Makefile.default.cfg Makefile.cfg
    elif [[ -f Makefile.dreamcast.cfg ]]; then
      cp -n Makefile.dreamcast.cfg Makefile.cfg 2>/dev/null || cp Makefile.dreamcast.cfg Makefile.cfg
    fi
    # Parallel configure under MSYS often corrupts binutils BFD deps
    # (".deps/elf32-sparc.Plo: No such file"). Cap jobs for stability.
    JOBS="$(nproc 2>/dev/null || echo 4)"
    if [[ "$JOBS" -gt 4 ]]; then
      JOBS=4
    fi
    if [[ -f Makefile.cfg ]]; then
      if grep -q '^makejobs=' Makefile.cfg 2>/dev/null; then
        sed -i "s/^makejobs=.*/makejobs=$JOBS/" Makefile.cfg || true
      fi
      # Prefer curl; avoid GCC download_prerequisites (hardcodes broken MSYS wget)
      if grep -q '^#force_downloader=' Makefile.cfg 2>/dev/null; then
        sed -i 's/^#force_downloader=.*/force_downloader=curl/' Makefile.cfg || true
      elif ! grep -q '^force_downloader=' Makefile.cfg 2>/dev/null; then
        echo 'force_downloader=curl' >> Makefile.cfg
      else
        sed -i 's/^force_downloader=.*/force_downloader=curl/' Makefile.cfg || true
      fi
      if ! grep -q '^use_custom_dependencies=1' Makefile.cfg 2>/dev/null; then
        echo 'use_custom_dependencies=1' >> Makefile.cfg
      fi
    fi
    # If wget is broken, install curl-based shim (GCC download_prerequisites needs wget)
    if ! wget --version >/dev/null 2>&1; then
      echo "WARNING: system wget is broken; installing curl-based wget shim"
      SHIM_SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/wget_curl_shim.sh"
      if [[ -f "$SHIM_SRC" ]]; then
        if [[ -f /usr/bin/wget.exe ]]; then
          mv -f /usr/bin/wget.exe /usr/bin/wget.exe.broken || true
        fi
        cp -f "$SHIM_SRC" /usr/bin/wget
        chmod +x /usr/bin/wget
      fi
    fi
    # MSYS2 config.guess is x86_64-pc-msys, but dc-chain only ships host
    # patches under x86_64-pc-cygwin (fputs_unlocked fix for gcc sh.cc).
    if [[ -d patches/x86_64-pc-cygwin ]]; then
      mkdir -p patches/x86_64-pc-msys
      for f in patches/x86_64-pc-cygwin/*.diff; do
        [[ -f "$f" ]] || continue
        base="$(basename "$f")"
        if [[ ! -f "patches/x86_64-pc-msys/$base" ]]; then
          cp -f "$f" "patches/x86_64-pc-msys/$base"
          echo "    Mirrored host patch patches/x86_64-pc-msys/$base"
        fi
      done
      # If GCC sources already unpacked without the host patch, apply it now.
      for gcc_src in gcc-*/; do
        [[ -d "$gcc_src" ]] || continue
        ver="${gcc_src%/}"
        host_diff="patches/x86_64-pc-msys/${ver}.diff"
        sh_cc="${gcc_src}gcc/config/sh/sh.cc"
        if [[ -f "$host_diff" && -f "$sh_cc" ]] && ! grep -q 'fputs() and related are redefined' "$sh_cc" 2>/dev/null; then
          echo "    Applying missed host patch $host_diff"
          patch -N -d "$gcc_src" -p1 < "$host_diff" || true
        fi
      done
    fi
    # Wipe corrupted/partial binutils trees before resume (stale BFD .deps).
    for d in build-binutils-*; do
      [[ -d "$d" ]] || continue
      if [[ -f "$d/bfd/Makefile" ]] && grep -q 'elf32-sparc\.Plo' "$d/bfd/Makefile" 2>/dev/null \
         && [[ ! -f "$d/bfd/.deps/elf32-sparc.Plo" ]]; then
        echo "    Removing corrupted $d (stale BFD deps)"
        rm -rf "$d"
      fi
    done
    # Always wipe incomplete binutils if last log shows the known failure.
    if [[ -f logs/build-binutils-sh-elf-2.43.log ]] \
       && grep -q 'elf32-sparc\.Plo' logs/build-binutils-sh-elf-2.43.log 2>/dev/null; then
      echo "    Clearing build-binutils-sh-elf-2.43 after prior BFD dep failure"
      rm -rf build-binutils-sh-elf-2.43
    fi
    make
    echo "dc-chain finished."
  fi
fi

# ---------------------------------------------------------------------------
# environ.sh + build KOS
# ---------------------------------------------------------------------------
echo
echo "[4/6] Configuring environ.sh and building KallistiOS..."
cd "$KOS_BASE"
if [[ ! -f environ.sh ]]; then
  if [[ -f doc/environ.sh.sample ]]; then
    cp doc/environ.sh.sample environ.sh
    sed -i 's|^export KOS_BASE=.*|export KOS_BASE="'"$KOS_BASE"'"|' environ.sh || true
  else
    echo "ERROR: doc/environ.sh.sample missing"
    exit 1
  fi
fi

# Ensure paths point at our install root if sample uses defaults
# (sample already defaults to /opt/toolchains/dc/...)

# KOS environ_*.sh use patterns like [ -z "${KOS_SUBARCH}" ] which abort under
# set -u when the var is unset. Disable nounset while sourcing.
export KOS_SUBARCH="${KOS_SUBARCH:-pristine}"
# shellcheck disable=SC1091
set +u
source "$KOS_BASE/environ.sh"
set -u

if [[ ! -x /opt/toolchains/dc/sh-elf/bin/sh-elf-gcc && ! -x /opt/toolchains/dc/sh-elf/bin/sh-elf-gcc.exe ]]; then
  echo "ERROR: SH-4 cross-compiler missing at /opt/toolchains/dc/sh-elf"
  echo "install_kos.bat did not finish dc-chain yet (often 1-3+ hours)."
  echo "Re-run install_kos.bat and leave it running until it completes."
  exit 1
fi

if ! command -v kos-cc >/dev/null 2>&1; then
  echo "ERROR: kos-cc not available after sourcing environ.sh"
  echo "Check that dc-chain installed sh-elf under /opt/toolchains/dc/"
  exit 1
fi

# Host image tools (dcbumpgen, kmgenc, vqenc, makeip) look in /usr/local by default.
# Point them at MinGW UCRT64 (or mingw64) libpng/libjpeg and use that gcc to link.
HOST_MINGW_ROOT=""
if [[ -f /ucrt64/include/png.h && -f /ucrt64/include/jpeglib.h ]]; then
  HOST_MINGW_ROOT="/ucrt64"
elif [[ -f /mingw64/include/png.h && -f /mingw64/include/jpeglib.h ]]; then
  HOST_MINGW_ROOT="/mingw64"
fi
if [[ -n "$HOST_MINGW_ROOT" ]]; then
  echo "    Host PNG/JPEG libs: $HOST_MINGW_ROOT (for dcbumpgen/kmgenc/...)"
  export PATH="${HOST_MINGW_ROOT}/bin:${PATH}"
  for mf in utils/dcbumpgen/Makefile utils/kmgenc/Makefile utils/vqenc/Makefile utils/makeip/src/Makefile; do
    if [[ -f "$mf" ]]; then
      sed -i "s|-I/usr/local/include|-I${HOST_MINGW_ROOT}/include|g" "$mf"
      sed -i "s|-L/usr/local/lib|-L${HOST_MINGW_ROOT}/lib|g" "$mf"
    fi
  done
  # Prefer MinGW gcc for host $(CC) targets (ABI matches libpng/libjpeg packages).
  if [[ -x "${HOST_MINGW_ROOT}/bin/gcc.exe" || -x "${HOST_MINGW_ROOT}/bin/gcc" ]]; then
    export CC="${HOST_MINGW_ROOT}/bin/gcc"
  fi
  # Drop stale MSYS-built .o files (mixing with MinGW causes __getreent link errors).
  for d in utils/dcbumpgen utils/kmgenc utils/vqenc; do
    if [[ -d "$d" ]]; then
      rm -f "$d"/*.o "$d"/dcbumpgen "$d"/dcbumpgen.exe "$d"/kmgenc "$d"/kmgenc.exe "$d"/vqenc "$d"/vqenc.exe
    fi
  done
else
  echo "WARNING: libpng/jpeglib headers not found under /ucrt64 or /mingw64."
  echo "         dcbumpgen may fail; install mingw-w64-*-libpng and *-libjpeg-turbo."
fi

make -j"$(nproc 2>/dev/null || echo 4)"
echo "KallistiOS built."

# ---------------------------------------------------------------------------
# kos-ports (optional but useful)
# ---------------------------------------------------------------------------
echo
echo "[5/6] kos-ports (clone; full build-all is optional / long)..."
if [[ ! -d "$KOS_PORTS/.git" ]]; then
  git clone --recursive https://github.com/KallistiOS/kos-ports "$KOS_PORTS" || true
else
  git -C "$KOS_PORTS" pull --ff-only || true
fi
echo "Skipping kos-ports build-all by default (run $KOS_PORTS/utils/build-all.sh later if needed)."

# ---------------------------------------------------------------------------
# mkdcdisc (CDI images for Pyrite Build for Dreamcast)
# ---------------------------------------------------------------------------
echo
echo "[6/6] Building mkdcdisc (for .cdi output)..."
mkdcdisc_ok=0
export PATH="${DC_ROOT}/bin:/usr/bin:${PATH}"
if { command -v mkdcdisc >/dev/null 2>&1 || [[ -x "$DC_ROOT/bin/mkdcdisc.exe" || -x "$DC_ROOT/bin/mkdcdisc" ]]; } \
   && [[ "$FORCE" != "1" ]]; then
  echo "mkdcdisc already available: $(command -v mkdcdisc 2>/dev/null || echo "$DC_ROOT/bin/mkdcdisc")"
  mkdcdisc_ok=1
else
  # libisofs is not packaged for MSYS2; build from source under /usr (MSYS ABI).
  # MinGW/UCRT builds fail the iconv configure check on this platform.
  if ! pkg-config --exists libisofs-1 2>/dev/null; then
    echo "    Building libisofs (required by mkdcdisc)..."
    pacman -S --needed --noconfirm autotools libtool make libiconv-devel gcc meson 2>/dev/null || true
    libisofs_ver="1.5.6"
    libisofs_src="$DC_ROOT/src/libisofs-${libisofs_ver}"
    mkdir -p "$DC_ROOT/src"
    if [[ ! -d "$libisofs_src" ]]; then
      (
        cd "$DC_ROOT/src"
        curl -fsSL -o "libisofs-${libisofs_ver}.tar.gz" \
          "https://files.libburnia-project.org/releases/libisofs-${libisofs_ver}.tar.gz" \
          || true
        if [[ -f "libisofs-${libisofs_ver}.tar.gz" ]]; then
          tar xf "libisofs-${libisofs_ver}.tar.gz"
        fi
      )
    fi
    if [[ -d "$libisofs_src" ]]; then
      (
        cd "$libisofs_src"
        export PATH="/usr/bin:$PATH"
        unset PKG_CONFIG_PATH || true
        make distclean >/dev/null 2>&1 || true
        if [[ ! -f configure ]]; then
          ./bootstrap || true
        fi
        ./configure --prefix=/usr --disable-static
        if [[ -f libtool ]]; then
          sed -i.bak -e "s/allow_undefined=yes/allow_undefined=no/" libtool || true
        fi
        make -j"$(nproc 2>/dev/null || echo 4)"
        make install
      ) || echo "WARNING: libisofs build failed."
    else
      echo "WARNING: could not download libisofs source."
    fi
  fi

  if [[ ! -d "$MKDCDISC_DIR/.git" ]]; then
    git clone https://gitlab.com/simulant/mkdcdisc.git "$MKDCDISC_DIR" || \
      git clone https://github.com/Mark65537/mkdcdisc.git "$MKDCDISC_DIR" || true
  fi
  if [[ -d "$MKDCDISC_DIR" ]]; then
    cd "$MKDCDISC_DIR"
    # Match libisofs: build with MSYS gcc, not UCRT MinGW.
    export PATH="/usr/bin:${PATH}"
    unset CC CXX PKG_CONFIG_PATH || true
    rm -rf builddir
    if meson setup builddir --prefix="$DC_ROOT" \
       && meson compile -C builddir; then
      meson install -C builddir || true
      if [[ -x builddir/mkdcdisc ]]; then
        mkdir -p "$DC_ROOT/bin"
        cp -f builddir/mkdcdisc "$DC_ROOT/bin/mkdcdisc"
        echo "Installed $DC_ROOT/bin/mkdcdisc"
        mkdcdisc_ok=1
      elif [[ -x builddir/mkdcdisc.exe ]]; then
        mkdir -p "$DC_ROOT/bin"
        cp -f builddir/mkdcdisc.exe "$DC_ROOT/bin/mkdcdisc.exe"
        echo "Installed $DC_ROOT/bin/mkdcdisc.exe"
        mkdcdisc_ok=1
      fi
    else
      echo "WARNING: mkdcdisc meson build failed."
    fi
  else
    echo "WARNING: could not clone mkdcdisc — ELF builds will still work; CDI packaging skipped."
  fi
fi

if [[ "$mkdcdisc_ok" != "1" ]]; then
  echo "WARNING: mkdcdisc not installed — Dreamcast ELF builds still work; .cdi packaging skipped."
fi

echo
echo "OK: KallistiOS is installed at $KOS_BASE"
echo "    source $KOS_BASE/environ.sh"
exit 0
