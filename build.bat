@echo off
setlocal
cd /d "%~dp0"

echo Building Pyrite64 editor...
echo.

REM Use a clean toolchain so CMake doesn't pick up N-Gage/Symbian/devkitPro GCC.
REM Prefer UCRT64 (official docs), then MINGW64, then C:\mingw64.
set "TOOLPATH="
if exist "C:\msys64\ucrt64\bin\g++.exe" (
    set "TOOLPATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin"
)
if exist "C:\msys64\mingw64\bin\g++.exe" (
    if not defined TOOLPATH set "TOOLPATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin"
)
if exist "C:\mingw64\bin\g++.exe" (
    if not defined TOOLPATH set "TOOLPATH=C:\mingw64\bin"
)
if defined TOOLPATH (
    set "PATH=%TOOLPATH%;%PATH%"
    echo Using toolchain: %TOOLPATH%
) else (
    echo No MSYS2 UCRT64/MINGW64 found. Run install_deps.bat first, or add C:\msys64\ucrt64\bin to PATH.
)
echo.

REM If build was configured from WSL/Linux, cache has wrong paths - remove it.
if exist "build\CMakeCache.txt" (
    findstr /C:"/home/" build\CMakeCache.txt >nul 2>nul
    if not errorlevel 1 (
        echo Removing stale build folder from WSL/Linux configure...
        rmdir /s /q build
        echo.
    )
)

REM Configure with Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo.
    echo Configure failed.
    echo Run install_deps.bat to install MSYS2 UCRT64 toolchain, or add C:\msys64\ucrt64\bin to PATH.
    exit /b 1
)

REM Build
cmake --build build --config Release
if errorlevel 1 (
    echo.
    echo Build failed.
    exit /b 1
)

echo.
echo Build succeeded. Executable: pyrite64.exe in project root.
exit /b 0
