#!/usr/bin/env bash
# Install the Bangla Keyboard IBus engine system-wide (Debian/Ubuntu and most
# distros with IBus). Builds first, copies the binary + component XML, restarts
# IBus. Run with sudo:  sudo ./install.sh
set -euo pipefail
cd "$(dirname "$0")"

BIN=/usr/lib/ibus/ibus-engine-bangla
XML=/usr/share/ibus/component/bangla.xml

if [ "$(id -u)" -ne 0 ]; then echo "Run with sudo: sudo ./install.sh"; exit 1; fi

echo "[1/4] build"
# build as the invoking (non-root) user if possible, else here
if [ -n "${SUDO_USER:-}" ]; then sudo -u "$SUDO_USER" ./build.sh; else ./build.sh; fi

echo "[2/4] install binary + icons -> $BIN"
install -Dm755 dist/ibus-engine-bangla "$BIN"
install -Dm644 icons/bangla-unicode.png /usr/share/ibus/icons/bangla-unicode.png
install -Dm644 icons/bangla-classic.png /usr/share/ibus/icons/bangla-classic.png

echo "[3/4] install component -> $XML"
install -d "$(dirname "$XML")"
cat > "$XML" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<component>
  <name>org.freedesktop.IBus.Bangla</name>
  <description>Bangla Keyboard (Unicode + Classic)</description>
  <exec>$BIN --ibus</exec>
  <version>1.1.0</version>
  <author>BiswasHost</author>
  <license>MIT</license>
  <homepage>https://github.com/wpexpertinbd/bangla-keyboard</homepage>
  <textdomain>ibus-bangla</textdomain>
  <icon>/usr/share/ibus/icons/bangla-unicode.png</icon>
  <engines>
    <engine>
      <name>bangla-unicode</name>
      <language>bn</language><license>MIT</license><author>BiswasHost</author><layout>us</layout>
      <longname>Bangla Unicode</longname>
      <description>Bangla (Unicode) — fixed Windows-style layout</description>
      <icon>/usr/share/ibus/icons/bangla-unicode.png</icon>
      <rank>1</rank>
    </engine>
    <engine>
      <name>bangla-classic</name>
      <language>bn</language><license>MIT</license><author>BiswasHost</author><layout>us</layout>
      <longname>Bangla Classic</longname>
      <description>Bangla (legacy ANSI — needs a legacy ANSI Bangla font)</description>
      <icon>/usr/share/ibus/icons/bangla-classic.png</icon>
      <rank>0</rank>
    </engine>
  </engines>
</component>
EOF

echo "[4/4] restart IBus registry"
if [ -n "${SUDO_USER:-}" ]; then sudo -u "$SUDO_USER" ibus write-cache --system 2>/dev/null || true; fi

cat <<'MSG'

Installed. To finish:
  1) ibus restart                (or log out/in)
  2) Add the input source: GNOME Settings -> Keyboard -> Input Sources -> +
     -> Bangla -> "Bangla Unicode" (and/or "Bangla Classic").
  3) Switch with Super+Space and type on a US-QWERTY layout.

Bangla Classic renders only in a legacy ANSI ("MJ"-style) Bangla font — install
one from your own legitimate source and select it in your app.
MSG
