@echo off
setlocal EnableExtensions
echo.
echo Pyrite64 - KallistiOS (Dreamcast) installer
echo Based on: https://www.dreamcast.wiki/Getting_Started_with_Dreamcast_development
echo.
echo This builds the SH-4 cross toolchain + KallistiOS under MSYS2.
echo Expect 1-3+ hours depending on your CPU. Leave the window open.
echo.
echo Install path: C:\msys64\opt\toolchains\dc\kos
echo.

set "MSYS2_PATH=C:\msys64"
set "BASH=%MSYS2_PATH%\usr\bin\bash.exe"
set "KOS_WIN=%MSYS2_PATH%\opt\toolchains\dc\kos"

if not exist "%BASH%" (
    echo MSYS2 not found at %MSYS2_PATH%
    echo.
    echo Install MSYS2 first ^(default path C:\msys64^), or run install_deps.bat.
    echo   https://www.msys2.org/
    echo.
    start https://www.msys2.org/ 2>nul
    exit /b 1
)

where git >nul 2>nul
if errorlevel 1 (
    echo Git not found in PATH. Install Git for Windows, then re-run this script.
    start https://git-scm.com/ 2>nul
    exit /b 1
)

cd /d "%~dp0"

REM Optional args: skip-toolchain | force
set "KOS_ARGS=%*"

echo Starting MSYS2 install script...
echo.

"%BASH%" -l -c "cd \"$(cygpath -u '%CD%')\" && bash ./data/scripts/install_kos.sh %KOS_ARGS%"
set "ERR=%ERRORLEVEL%"

if not "%ERR%"=="0" (
    echo.
    echo ---------------------------------------------------------------------------
    echo KallistiOS install FAILED ^(exit %ERR%^).
    echo Check the log above. You can re-run this script; it resumes where possible.
    echo ---------------------------------------------------------------------------
    exit /b %ERR%
)

REM Help the Pyrite editor / Windows shells find KOS without sourcing environ.sh
if exist "%KOS_WIN%\environ.sh" (
    setx KOS_BASE "%KOS_WIN%" >nul
    echo.
    echo Set user environment variable KOS_BASE=%KOS_WIN%
    echo Open a NEW terminal / restart the Pyrite editor so it picks this up.
)

echo.
echo ---------------------------------------------------------------------------
echo Done. Dreamcast toolchain ready.
echo.
echo In MSYS2 ^(to build manually^):
echo   source /opt/toolchains/dc/kos/environ.sh
echo.
echo In Pyrite64 editor:
echo   Build -^> Build for Dreamcast
echo.
echo Optional CDI packaging: ensure mkdcdisc is on PATH ^(installer tries to build it^).
echo ---------------------------------------------------------------------------
exit /b 0
