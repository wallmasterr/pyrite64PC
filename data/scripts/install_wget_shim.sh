#!/bin/sh
set -e
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
if [ -f /usr/bin/wget.exe ]; then
  mv -f /usr/bin/wget.exe /usr/bin/wget.exe.broken
fi
cp -f "$SCRIPT_DIR/wget_curl_shim.sh" /usr/bin/wget
chmod +x /usr/bin/wget
# smoke test: same flags GCC download_prerequisites uses
cd /tmp
rm -f gmp-6.2.1.tar.bz2
wget --no-verbose https://gcc.gnu.org/pub/gcc/infrastructure/gmp-6.2.1.tar.bz2
ls -la gmp-6.2.1.tar.bz2
echo WGET_SHIM_OK
