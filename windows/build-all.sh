#!/usr/bin/env bash
# Build every Windows artifact into windows/dist/ with MinGW g++.
#
# Toolchains (w64devkit ships the TSF headers; both DLLs also build with MSVC):
#   GXX64 = x64 g++  (default: g++)         -> exes + BanglaKeyboard.dll   (x64)
#   GXX32 = i686 g++ (optional)             -> BanglaKeyboard32.dll        (x86)
# Example:
#   GXX64=/path/w64devkit/bin/g++ \
#   GXX32=/path/w64devkit32/bin/g++ ./build-all.sh
#
# Each compiler is invoked with -B<its own bin> so it uses its own assembler/
# linker even if the other toolchain is also on PATH (x64 and x86 `as` differ).
set -euo pipefail
cd "$(dirname "$0")"
GXX64="${GXX64:-${GXX:-g++}}"
GXX32="${GXX32:-}"
mkdir -p dist

tooldir() { dirname "$(command -v "$1" 2>/dev/null || echo "$1")"; }

DLL_SRC="engine/engine.cpp tsf/Globals.cpp tsf/TextService.cpp tsf/Dll.cpp tsf/BanglaKeyboard.def"
DLL_FLAGS="-std=c++17 -O2 -shared -static -static-libgcc -static-libstdc++ -DUNICODE -D_UNICODE -Itsf -Iengine"
DLL_LIBS="-lole32 -loleaut32 -luuid -ladvapi32 -luser32"

B64="$(tooldir "$GXX64")"
echo "[x64] enginetest.exe + bangla-demo.exe + bangla-tray.exe"
"$GXX64" -B"$B64" -std=c++17 -O2 -static -finput-charset=UTF-8 engine/test.cpp engine/engine.cpp -o dist/enginetest.exe
"$GXX64" -B"$B64" -std=c++17 -O2 -static -finput-charset=UTF-8 engine/demo.cpp engine/engine.cpp -o dist/bangla-demo.exe
"$GXX64" -B"$B64" -std=c++17 -O2 -static -mwindows -municode -finput-charset=UTF-8 tray/tray.cpp engine/engine.cpp -o dist/bangla-tray.exe -lgdi32 -luser32 -lshell32
echo "[x64] BanglaKeyboard.dll + loadtest.exe"
"$GXX64" -B"$B64" $DLL_FLAGS $DLL_SRC -o dist/BanglaKeyboard.dll $DLL_LIBS
"$GXX64" -B"$B64" -std=c++17 -O2 -static tsf/loadtest.cpp -o dist/loadtest.exe -lole32 -loleaut32 -luuid

if [ -n "$GXX32" ]; then
  B32="$(tooldir "$GXX32")"
  echo "[x86] BanglaKeyboard32.dll + loadtest32.exe"
  "$GXX32" -B"$B32" $DLL_FLAGS $DLL_SRC -o dist/BanglaKeyboard32.dll $DLL_LIBS
  "$GXX32" -B"$B32" -std=c++17 -O2 -static tsf/loadtest.cpp -o dist/loadtest32.exe -lole32 -loleaut32 -luuid
else
  echo "[x86] skipped (set GXX32 to an i686 g++ to build the 32-bit DLL)"
fi

echo "Done -> windows/dist/"
