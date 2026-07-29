# Run in: C:\msys64\mingw64.exe

# Enable bash 'strict mode'
set -euo pipefail
IFS=$'\n\t'

zipfile="gcc-toolchain-mips64-win64.zip"
sdkpath="/pyrite64-sdk"
workpath="/pyrite64-tmp"

mkdir -p "$workpath"
cd "$workpath"

export N64_INST="${N64_INST:-$sdkpath}"

echo "Updating MSYS2 environment..."

if pacman -Qu | grep -Eq '^(msys2-runtime|bash|pacman)\s'; then
    msysUpdate=true
else
    msysUpdate=false
fi

pacman -Syyu --noconfirm

if $msysUpdate; then
    echo "Core MSYS2 components were updated. Please close this window and start the installation again."
    exit 1
fi

pacman -S unzip base-devel mingw-w64-x86_64-gcc mingw-w64-x86_64-make git --noconfirm --needed

echo "Building Libdragon environment..."

# download libdragon toolchain
if [ -e $zipfile ]; then
    echo "Toolchain already downloaded"
else
    wget "https://github.com/DragonMinded/libdragon/releases/download/toolchain-continuous-prerelease/gcc-toolchain-mips64-win64.zip" -O $zipfile
fi

if [ -e $sdkpath ]; then
    echo "Toolchain already extracted"
else
    echo "Extracting toolchain..."
    # rm -rf $sdkpath
    unzip -q $zipfile -d $sdkpath
fi

# Sanity check: Verify that gcc exists
if [ ! -x "$sdkpath/bin/mips64-elf-gcc.exe" ]; then
    echo "ERROR: Toolchain installation appears incomplete."
    echo "Expected file not found: $sdkpath/bin/mips64-elf-gcc.exe"
    echo "Toolchain ZIP file may be corrupted or only partially downloaded."

    rm -rf "$zipfile" "$sdkpath"

    echo "Please re-run the setup to download and extract the toolchain again."
    exit 1
fi

echo "Toolchain installation in '$N64_INST' looks OK"

# download libdragon itself
if [ -e "libdragon" ]; then
    echo "Libdragon already downloaded"
else
    git clone -b preview https://github.com/DragonMinded/libdragon.git
fi

cd libdragon

git fetch origin preview
git checkout preview
git reset --hard origin/preview
make clean && make -C tools clean

# Rebuild if tools missing, forced, or headers lack APIs the editor requires
need_libdragon=0
if [[ ! -f "$sdkpath/bin/n64tool.exe" || "${FORCE_UPDATE:-}" == "true" ]]; then
  need_libdragon=1
fi
if [[ ! -f "$sdkpath/mips64-elf/include/fgeom.h" ]] \
   || ! grep -q 'operator==(fm_vec3_t' "$sdkpath/mips64-elf/include/fgeom.h" 2>/dev/null; then
  echo "Installed libdragon headers are out of date (missing fm_vec3_t operator==) — rebuilding"
  need_libdragon=1
fi
if [[ ! -f "$sdkpath/mips64-elf/include/rspq.h" ]] \
   || ! grep -q 'rspq_block_begin_reuse' "$sdkpath/mips64-elf/include/rspq.h" 2>/dev/null; then
  echo "Installed libdragon headers are out of date (missing rspq_block_begin_reuse) — rebuilding"
  need_libdragon=1
fi

if [[ "$need_libdragon" == "1" ]]; then
    echo "Building libdragon..."
    make -j6 libdragon && make -j6 tools
    make install || sudo -E make install
    make -C tools install || sudo -E make -C tools install
    # Build an example as sanity check
    make -C examples/brew-volley clean
    make -C examples/brew-volley
else
    echo "Libdragon already installed and up to date"
fi

cd ..

# download tiny3d
if [ -e "tiny3d" ]; then
    echo "Tiny3D already downloaded"
else
    git clone https://github.com/HailToDodongo/tiny3d.git
fi

cd tiny3d

git fetch origin
git pull --ff-only || true
make clean

need_tiny3d=0
if [[ ! -f "$sdkpath/bin/gltf_to_t3d.exe" || "${FORCE_UPDATE:-}" == "true" ]]; then
  need_tiny3d=1
fi
if [[ ! -f "$sdkpath/mips64-elf/include/t3d/t3d.h" ]] \
   || ! grep -q 't3d_state_set_lighting_mode' "$sdkpath/mips64-elf/include/t3d/t3d.h" 2>/dev/null; then
  echo "Installed tiny3d headers are out of date — rebuilding"
  need_tiny3d=1
fi

# Build Tiny3D
if [[ "$need_tiny3d" == "1" ]]; then
    echo "Building Tiny3D..."
    make -j6
    make install || sudo -E make install
else
    echo "Tiny3D already installed and up to date"
fi

# Tools
echo "Building tools..."
make -C tools/gltf_importer -j6
make -C tools/gltf_importer install || sudo -E make -C tools/gltf_importer install

cd ..

echo "Installation complete!"
sleep 2
