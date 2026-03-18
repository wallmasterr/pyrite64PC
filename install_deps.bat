@echo off
setlocal
echo.
echo Pyrite64 editor - dependency installer
echo Based on: https://hailtododongo.github.io/pyrite64/docs/dev/build.html
echo.

REM ---------------------------------------------------------------------------
REM 1. MSYS2
REM ---------------------------------------------------------------------------
set "MSYS2_PATH=C:\msys64"
set "BASH=%MSYS2_PATH%\usr\bin\bash.exe"

if not exist "%BASH%" (
    echo MSYS2 not found at %MSYS2_PATH%
    echo.
    echo Please install MSYS2 first:
    echo   1. Download: https://www.msys2.org/
    echo   2. Run the installer and use default path C:\msys64
    echo   3. Close the terminal it opens, then run this script again.
    echo.
    start https://www.msys2.org/ 2>nul
    exit /b 1
)

echo [1/3] MSYS2 found. Installing UCRT64 toolchain and build tools...
echo      This may take a few minutes.
echo.

set "MSYSTEM=UCRT64"
"%BASH%" -l -c "pacman -S -y --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja"
if errorlevel 1 (
    echo.
    echo pacman failed. Try opening "MSYS2 UCRT64" from Start menu and run:
    echo   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
    exit /b 1
)

echo.
echo UCRT64 packages installed.
echo.

REM ---------------------------------------------------------------------------
REM 2. Git LFS
REM ---------------------------------------------------------------------------
echo [2/3] Checking Git LFS...

where git >nul 2>nul
if errorlevel 1 (
    echo Git not found in PATH. Install Git for Windows from https://git-scm.com/
    echo It includes Git LFS. Then run this script again.
    start https://git-scm.com/ 2>nul
    exit /b 1
)

git lfs version >nul 2>nul
if errorlevel 1 (
    echo Git LFS not found. Install from https://git-lfs.com/ and run: git lfs install
    start https://git-lfs.com/ 2>nul
    exit /b 1
)

git lfs install
echo Git LFS ready.
echo.

REM ---------------------------------------------------------------------------
REM 3. Submodules (in repo only)
REM ---------------------------------------------------------------------------
echo [3/3] Git submodules...

cd /d "%~dp0"
if exist ".git" (
    git submodule update --init --recursive
    if errorlevel 1 (
        echo Warning: git submodule update had issues. Check and run manually if needed.
    ) else (
        echo Submodules updated.
    )
) else (
    echo Not a git repo; skipping submodules.
)

echo.
echo ---------------------------------------------------------------------------
echo Done. To build the editor:
echo   1. Open "MSYS2 UCRT64" from the Start menu
echo   2. cd to this project folder
echo   3. Run: cmake --preset windows-gcc-release
echo   4. Run: cmake --build --preset windows-gcc-release
echo.
echo Or use build.bat from a normal Command Prompt after adding to PATH:
echo   set PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%%PATH%%
echo   build.bat
echo ---------------------------------------------------------------------------
exit /b 0
