#!/bin/bash
# wadiwad macOS external dumper launcher
set -e

# ---- config ----
REPO="zyit0000/yo"
DUMPER_NAME="dumper"
INSTALL_DIR="$HOME/.wadiwad_external"
DOWNLOAD_BASE="https://github.com/$REPO/releases/latest/download"
# -----------------

echo "[*] wadiwad macOS external dumper launcher"

# create install dir
mkdir -p "$INSTALL_DIR"

# always delete old dumper
echo "[*] Removing old dumper..."
rm -f "$INSTALL_DIR/$DUMPER_NAME"

# download latest dumper
echo "[*] Downloading latest $DUMPER_NAME..."
curl -L -o "$INSTALL_DIR/$DUMPER_NAME" "$DOWNLOAD_BASE/$DUMPER_NAME"
if [ ! -f "$INSTALL_DIR/$DUMPER_NAME" ]; then
    echo "[-] Failed to download $DUMPER_NAME"
    exit 1
fi
chmod +x "$INSTALL_DIR/$DUMPER_NAME"
echo "[+] $DUMPER_NAME installed"

echo "[*] Launching dumper (requires sudo)..."
sudo "$INSTALL_DIR/$DUMPER_NAME"
