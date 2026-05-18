#!/bin/bash
# wadiwad macOS setup — downloads latest executor.dylib + send.sh from GitHub release
set -e

# ---- config ----
REPO="zyit0000/yo"
DYLIB_NAME="executor.dylib"
INSTALL_DIR="$HOME/.wadiwad"
DOWNLOAD_BASE="https://github.com/$REPO/releases/latest/download"
# -----------------

echo "[*] wadiwad macOS setup"
echo "[*] Install dir: $INSTALL_DIR"

# create install dir
mkdir -p "$INSTALL_DIR"

# remove old files
echo "[*] Removing old files..."
rm -f "$INSTALL_DIR/$DYLIB_NAME"
rm -f "$INSTALL_DIR/send.sh"

# download latest dylib
echo "[*] Downloading $DYLIB_NAME..."
curl -L -o "$INSTALL_DIR/$DYLIB_NAME" "$DOWNLOAD_BASE/$DYLIB_NAME"
if [ ! -f "$INSTALL_DIR/$DYLIB_NAME" ]; then
    echo "[-] Failed to download $DYLIB_NAME"
    exit 1
fi
echo "[+] $DYLIB_NAME installed"

# download latest send.sh
echo "[*] Downloading send.sh..."
curl -L -o "$INSTALL_DIR/send.sh" "$DOWNLOAD_BASE/send.sh"
if [ ! -f "$INSTALL_DIR/send.sh" ]; then
    echo "[-] Failed to download send.sh"
    exit 1
fi
chmod +x "$INSTALL_DIR/send.sh"
echo "[+] send.sh installed"

echo ""
echo "[+] Setup complete!"
echo "[+] Files installed to $INSTALL_DIR"
echo "[+] Run start.sh to launch"
