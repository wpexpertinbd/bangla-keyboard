#!/bin/bash
# Build Bangla Keyboard Layout for Mac  (.pkg + .dmg)
set -e
export COPYFILE_DISABLE=1
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PKGID="com.biswashost.bangla-keyboard"
VERSION="${1:-1.0.0}"
BUILD="$ROOT/.build"; DIST="$ROOT/dist"
rm -rf "$BUILD"; mkdir -p "$BUILD/payload/Library/Keyboard Layouts" "$BUILD/payload/Library/Fonts" "$DIST"

cp "$ROOT/src/keylayouts/"*.keylayout "$ROOT/src/keylayouts/"*.icns "$BUILD/payload/Library/Keyboard Layouts/"
cp "$ROOT/src/fonts/"*.ttf "$BUILD/payload/Library/Fonts/"
find "$BUILD/payload" -name '._*' -delete; find "$BUILD/payload" -name '.DS_Store' -delete
chmod -R go-w "$BUILD/payload"; find "$BUILD/payload" -type f -exec chmod 644 {} \;; find "$BUILD/payload" -type d -exec chmod 755 {} \;
xattr -cr "$BUILD/payload" 2>/dev/null || true

pkgbuild --root "$BUILD/payload" --identifier "$PKGID" --version "$VERSION" \
  --scripts "$HERE/scripts" --install-location "/" --ownership recommended \
  "$BUILD/component.pkg"

cat > "$BUILD/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Bangla Keyboard Layout for Mac $VERSION</title>
  <welcome file="welcome.html"/>
  <volume-check><allowed-os-versions><os-version min="11.0"/></allowed-os-versions></volume-check>
  <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <choices-outline><line choice="default"/></choices-outline>
  <choice id="default" title="Bangla Keyboard"><pkg-ref id="$PKGID"/></choice>
  <pkg-ref id="$PKGID" version="$VERSION">component.pkg</pkg-ref>
</installer-gui-script>
XML

# stamp the live version into a copy of the resources (welcome.html uses __VERSION__)
mkdir -p "$BUILD/resources"; cp "$HERE/resources/"* "$BUILD/resources/"
sed -i '' "s/__VERSION__/$VERSION/g" "$BUILD/resources/welcome.html"

productbuild --distribution "$BUILD/distribution.xml" --resources "$BUILD/resources" \
  --package-path "$BUILD" "$DIST/Bangla Keyboard.pkg"

# --- smart installer app: detects state, offers Install/Reinstall/Uninstall ---
APP="$BUILD/Bangla Keyboard Installer.app"
osacompile -o "$APP" "$HERE/installer.applescript"
RES="$APP/Contents/Resources"
cp "$DIST/Bangla Keyboard.pkg" "$RES/Bangla Keyboard.pkg"          # embed the installer
cp "$ROOT/src/keylayouts/Bangla Unicode.icns" "$RES/applet.icns"  # app icon
PB=/usr/libexec/PlistBuddy; PL="$APP/Contents/Info.plist"
$PB -c "Set :CFBundleName 'Bangla Keyboard Installer'" "$PL" 2>/dev/null || true
$PB -c "Add :CFBundleShortVersionString string $VERSION" "$PL" 2>/dev/null || $PB -c "Set :CFBundleShortVersionString $VERSION" "$PL" 2>/dev/null || true
$PB -c "Add :LSMinimumSystemVersion string 11.0" "$PL" 2>/dev/null || true
$PB -c "Add :NSHumanReadableCopyright string 'BiswasHost - https://www.biswashost.com'" "$PL" 2>/dev/null || true
# Re-sign ad-hoc AFTER all bundle edits — osacompile's signature is invalidated
# by embedding the pkg / icon / Info.plist edits, which makes a downloaded
# (quarantined) copy read as "damaged" on Apple Silicon. A valid ad-hoc sig
# turns that into the normal "unidentified developer" (Open Anyway) flow.
codesign --force --deep --sign - "$APP"
codesign --verify --deep "$APP" || { echo "codesign verify FAILED"; exit 1; }
touch "$APP"
cp -R "$APP" "$DIST/"   # also drop the app in dist/ for direct use

# DMG (the app is self-contained — pkg is embedded inside it)
rm -rf "$BUILD/dmg"; mkdir -p "$BUILD/dmg"
cp -R "$APP" "$BUILD/dmg/"
cp "$HERE/dmg-readme.txt" "$BUILD/dmg/Read Me.txt" 2>/dev/null || cp "$ROOT/README.md" "$BUILD/dmg/Read Me.txt" 2>/dev/null || true
find "$BUILD/dmg" -name '._*' -delete; xattr -cr "$BUILD/dmg" 2>/dev/null || true
hdiutil create -volname "Bangla Keyboard" -srcfolder "$BUILD/dmg" -ov -format UDZO "$DIST/Bangla Keyboard.dmg" >/dev/null
rm -rf "$BUILD"
echo "Built:"; ls -lh "$DIST"
