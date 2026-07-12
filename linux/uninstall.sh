#!/usr/bin/env bash
# Remove the Bangla Keyboard IBus engine.  Run with sudo:  sudo ./uninstall.sh
set -euo pipefail
if [ "$(id -u)" -ne 0 ]; then echo "Run with sudo: sudo ./uninstall.sh"; exit 1; fi
rm -f /usr/lib/ibus/ibus-engine-bangla
rm -f /usr/share/ibus/component/bangla.xml
rm -f /usr/share/ibus/icons/bangla-unicode.png /usr/share/ibus/icons/bangla-classic.png
echo "Removed. Run 'ibus restart' (or log out/in), and drop the input source in"
echo "Settings -> Keyboard -> Input Sources if it's still listed."
