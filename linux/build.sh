#!/usr/bin/env bash
# Build the Linux IBus engine into linux/dist/. Reuses the shared C++ KLEngine
# from ../windows/engine (the SAME engine as macOS + Windows — byte-identical
# output). Also writes a local component XML pointing at the just-built binary so
# you can test standalone (see linux/README.md) without a system install.
#
# Needs: build-essential, pkg-config, libibus-1.0-dev  (Debian/Ubuntu:
#   sudo apt install build-essential pkg-config libibus-1.0-dev)
set -euo pipefail
cd "$(dirname "$0")"                     # linux/
ENGINE=../windows/engine                 # the shared KLEngine lives here
mkdir -p dist

echo "[1/3] engine self-test (shared KLEngine on Linux)"
g++ -std=c++17 -O2 -I"$ENGINE" "$ENGINE/klengine_test.cpp" "$ENGINE/klengine.cpp" -o dist/kltest
dist/kltest | grep -E '^[0-9]+/[0-9]+' | sed 's/^/      /'

echo "[2/3] ibus-engine-bangla"
g++ -std=c++17 -O2 -I"$ENGINE" $(pkg-config --cflags ibus-1.0) \
    ibus-bangla.cpp "$ENGINE/klengine.cpp" \
    -o dist/ibus-engine-bangla $(pkg-config --libs ibus-1.0)

echo "[3/3] local component XML (standalone test)"
cat > dist/bangla.xml <<EOF
<?xml version="1.0" encoding="utf-8"?>
<component>
  <name>org.freedesktop.IBus.Bangla</name>
  <description>Bangla Keyboard (Unicode + Classic)</description>
  <exec>$(pwd)/dist/ibus-engine-bangla --ibus</exec>
  <version>1.1.0</version>
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
      <icon>$(pwd)/icons/bangla-unicode.png</icon>
      <rank>1</rank>
    </engine>
    <engine>
      <name>bangla-classic</name>
      <language>bn</language><license>MIT</license><author>BiswasHost</author><layout>us</layout>
      <longname>Classic</longname>
      <description>Bangla (legacy ANSI — needs a legacy ANSI Bangla font)</description>
      <icon>$(pwd)/icons/bangla-classic.png</icon>
      <rank>0</rank>
    </engine>
  </engines>
</component>
EOF

echo "Done -> linux/dist/ (ibus-engine-bangla + bangla.xml)"
