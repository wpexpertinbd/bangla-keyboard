@echo off
REM Build every Windows artifact into windows\dist\ with MinGW g++.
REM   set GXX64=...x64 g++    (default: g++)   -> exes + BanglaKeyboard.dll (x64)
REM   set GXX32=...i686 g++   (optional)       -> BanglaKeyboard32.dll      (x86)
REM w64devkit (github.com/skeeto/w64devkit) ships the TSF headers (msctf.h).
REM Each g++ is invoked with -B<its own bin> so it uses its own assembler/linker
REM even when both toolchains are on PATH. Both DLLs also build with MSVC + SDK.
setlocal enabledelayedexpansion
cd /d "%~dp0"
if "%GXX64%"=="" set "GXX64=g++"
where %GXX64% >nul 2>nul || (echo Need x64 MinGW g++ on PATH or set GXX64. & exit /b 1)
if not exist dist mkdir dist

REM resolve each compiler's bin directory (searches PATH if GXX is a bare name)
for %%I in ("%GXX64%") do set "B64=%%~dp$PATH:I"
if "%B64%"=="" for %%I in ("%GXX64%") do set "B64=%%~dpI"

set "SRC=engine\engine.cpp tsf\Globals.cpp tsf\TextService.cpp tsf\Dll.cpp tsf\BanglaKeyboard.def"
set "FLAGS=-std=c++17 -O2 -shared -static -static-libgcc -static-libstdc++ -DUNICODE -D_UNICODE -Itsf -Iengine"
set "LIBS=-lole32 -loleaut32 -luuid -ladvapi32 -luser32"

echo [x64] enginetest.exe + bangla-demo.exe + bangla-tray.exe
%GXX64% -B"%B64%." -std=c++17 -O2 -static -finput-charset=UTF-8 engine\test.cpp engine\engine.cpp -o dist\enginetest.exe || goto :fail
%GXX64% -B"%B64%." -std=c++17 -O2 -static -finput-charset=UTF-8 engine\demo.cpp engine\engine.cpp -o dist\bangla-demo.exe || goto :fail
%GXX64% -B"%B64%." -std=c++17 -O2 -static -mwindows -municode -finput-charset=UTF-8 tray\tray.cpp engine\engine.cpp -o dist\bangla-tray.exe -lgdi32 -luser32 -lshell32 || goto :fail
echo [x64] BanglaKeyboard.dll + loadtest.exe
%GXX64% -B"%B64%." %FLAGS% %SRC% -o dist\BanglaKeyboard.dll %LIBS% || goto :fail
%GXX64% -B"%B64%." -std=c++17 -O2 -static tsf\loadtest.cpp -o dist\loadtest.exe -lole32 -loleaut32 -luuid || goto :fail

if not "%GXX32%"=="" (
    for %%I in ("%GXX32%") do set "B32=%%~dp$PATH:I"
    if "!B32!"=="" for %%I in ("%GXX32%") do set "B32=%%~dpI"
    echo [x86] BanglaKeyboard32.dll + loadtest32.exe
    %GXX32% -B"!B32!." %FLAGS% %SRC% -o dist\BanglaKeyboard32.dll %LIBS% || goto :fail
    %GXX32% -B"!B32!." -std=c++17 -O2 -static tsf\loadtest.cpp -o dist\loadtest32.exe -lole32 -loleaut32 -luuid || goto :fail
) else (
    echo [x86] skipped ^(set GXX32 to an i686 g++ to build the 32-bit DLL^)
)

echo.
echo Done. Artifacts in windows\dist\  ^(register with installer\register.bat, as admin^)
exit /b 0
:fail
echo BUILD FAILED
exit /b 1
