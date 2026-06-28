#!/bin/bash
# Build "Bangla Voice.app" — the menu-bar voice-typing companion.
# Usage: ./build.sh [version]   (default 1.0.0)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
VERSION="${1:-1.0.0}"
APPID="com.biswashost.banglavoice"
OUT="$HERE/dist"
APP="$OUT/Bangla Voice.app"

rm -rf "$OUT"; mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

# compile (universal arm64+x86_64 where the SDK allows; arm64 minimum)
swiftc -O "$HERE/Sources/"*.swift -o "$APP/Contents/MacOS/BanglaVoice" \
  -framework AppKit -framework AVFoundation -framework CoreGraphics -framework Carbon

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>Bangla Voice</string>
  <key>CFBundleDisplayName</key><string>Bangla Voice</string>
  <key>CFBundleIdentifier</key><string>$APPID</string>
  <key>CFBundleExecutable</key><string>BanglaVoice</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>LSUIElement</key><true/>
  <key>NSMicrophoneUsageDescription</key>
  <string>Bangla Voice listens to your microphone only while a voice mode is on, to type what you say. Nothing is recorded or stored.</string>
  <key>NSSpeechRecognitionUsageDescription</key>
  <string>Bangla Voice uses on-device speech recognition for English voice typing. Nothing is recorded or stored.</string>
  <key>NSHumanReadableCopyright</key><string>BiswasHost — MIT. Not affiliated with any commercial keyboard/font vendor.</string>
</dict></plist>
PLIST

# Ad-hoc sign AFTER all bundle edits (Apple Silicon rejects a broken signature as
# "damaged"). Real distribution needs a Developer ID + notarization.
codesign --force --deep --sign - "$APP"
codesign --verify --deep "$APP" && echo "signed OK"

echo "Built: $APP  (v$VERSION)"
