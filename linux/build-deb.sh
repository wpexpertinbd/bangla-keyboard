#!/usr/bin/env bash
# Build a Debian/Ubuntu .deb of the Bangla Keyboard IBus engine.
#   ./build-deb.sh [version]      -> linux/dist/bangla-keyboard-ibus_<ver>_<arch>.deb
# Needs: dpkg-deb (dpkg-dev) + the build deps (see build.sh).
set -euo pipefail
cd "$(dirname "$0")"
VER="${1:-1.0.0}"
ARCH="$(dpkg --print-architecture 2>/dev/null || echo amd64)"

./build.sh                                    # -> dist/ibus-engine-bangla (+ self-test)

# Assemble in a NATIVE-fs temp dir: a Windows/WSL /mnt mount is always mode 777,
# which dpkg-deb rejects for the DEBIAN control dir.
SRC_ICONS="$(pwd)/icons"
BIN_BUILT="$(pwd)/dist/ibus-engine-bangla"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
ROOT="$TMP/bangla-keyboard-ibus_${VER}_${ARCH}"
install -Dm755 "$BIN_BUILT"                    "$ROOT/usr/lib/ibus/ibus-engine-bangla"
install -Dm644 "$SRC_ICONS/bangla-unicode.png" "$ROOT/usr/share/ibus/icons/bangla-unicode.png"
install -Dm644 "$SRC_ICONS/bangla-classic.png" "$ROOT/usr/share/ibus/icons/bangla-classic.png"

install -d "$ROOT/usr/share/ibus/component"
cat > "$ROOT/usr/share/ibus/component/bangla.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<component>
  <name>org.freedesktop.IBus.Bangla</name>
  <description>Bangla Keyboard (Unicode + Classic)</description>
  <exec>/usr/lib/ibus/ibus-engine-bangla --ibus</exec>
  <version>1.0.0</version>
  <author>BiswasHost</author>
  <license>MIT</license>
  <homepage>https://github.com/wpexpertinbd/bangla-keyboard</homepage>
  <textdomain>ibus-bangla</textdomain>
  <engines>
    <engine>
      <name>bangla-unicode</name>
      <language>bn</language><license>MIT</license><author>BiswasHost</author><layout>us</layout>
      <longname>Unicode</longname>
      <description>Bangla (Unicode) — fixed Windows-style layout</description>
      <icon>/usr/share/ibus/icons/bangla-unicode.png</icon>
      <rank>1</rank>
    </engine>
    <engine>
      <name>bangla-classic</name>
      <language>bn</language><license>MIT</license><author>BiswasHost</author><layout>us</layout>
      <longname>Classic</longname>
      <description>Bangla (legacy ANSI — needs a legacy ANSI Bangla font)</description>
      <icon>/usr/share/ibus/icons/bangla-classic.png</icon>
      <rank>0</rank>
    </engine>
  </engines>
</component>
XML

install -d "$ROOT/DEBIAN"
cat > "$ROOT/DEBIAN/control" <<CTL
Package: bangla-keyboard-ibus
Version: ${VER}
Architecture: ${ARCH}
Maintainer: BiswasHost <benjamin.biswas@gmail.com>
Depends: ibus, libstdc++6, libc6, libpulse0, libnotify-bin, libcurl4t64 | libcurl4
Section: utils
Priority: optional
Homepage: https://github.com/wpexpertinbd/bangla-keyboard
Description: Bangla Keyboard - fixed Windows-style Bangla layout (IBus)
 Bangla Unicode + Bangla Classic IBus engines that reproduce the macOS and
 Windows Bangla Keyboard exactly (prebase-vowel and reph reordering), driven by
 the same shared engine. US-QWERTY based, MIT licensed.
 .
 After install: log out and back in, then add "Bangla (Unicode)" (and/or
 "Bangla (Classic)") in Settings -> Keyboard -> Input Sources, and switch with
 Super+Space.
CTL

# No maintainer scripts: `ibus write-cache` needs the user session (fails under
# dpkg's root context); a log out / back in rebuilds the IBus registry anyway.

mkdir -p dist
dpkg-deb --build --root-owner-group "$ROOT" "$TMP/pkg.deb" >/dev/null
cp "$TMP/pkg.deb" "dist/bangla-keyboard-ibus_${VER}_${ARCH}.deb"
echo "built -> linux/dist/bangla-keyboard-ibus_${VER}_${ARCH}.deb"
