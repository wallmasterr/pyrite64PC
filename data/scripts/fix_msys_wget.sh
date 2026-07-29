#!/bin/sh
set -e
if [ -f /usr/bin/wget.exe ]; then
  mv -f /usr/bin/wget.exe /usr/bin/wget.exe.broken
fi
cat > /usr/bin/wget <<'EOF'
#!/bin/sh
# Shim: MSYS wget is broken on this machine (missing DLL). Use curl.
exec curl -L --fail "$@"
EOF
chmod +x /usr/bin/wget
wget -O /tmp/wtest.html https://example.com
echo WGET_SHIM_OK
