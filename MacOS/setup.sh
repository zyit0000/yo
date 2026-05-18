#!/bin/bash
# wadiwad macOS setup — downloads latest executor.dylib from GitHub release
set -e

# ---- config ----
REPO="zyit0000/yo"
DYLIB_NAME="executor.dylib"
INSTALL_DIR="$HOME/.wadiwad"
# -----------------

echo "[*] wadiwad macOS setup"

# detect architecture
ARCH=$(uname -m)
echo "[*] Architecture: $ARCH"

if [ "$ARCH" != "x86_64" ]; then
    echo "[!] This build is for Intel (x86_64) Macs."
    echo "[!] If you're on Apple Silicon, Rosetta will handle it."
    echo "[!] Install Rosetta: softwareupdate --install-rosetta"
fi

# create install dir
mkdir -p "$INSTALL_DIR"

# remove old dylib
if [ -f "$INSTALL_DIR/$DYLIB_NAME" ]; then
    echo "[*] Removing old dylib..."
    rm -f "$INSTALL_DIR/$DYLIB_NAME"
fi

# download latest from GitHub release
echo "[*] Downloading latest $DYLIB_NAME from $REPO..."
DOWNLOAD_URL="https://github.com/$REPO/releases/latest/download/$DYLIB_NAME"
curl -L -o "$INSTALL_DIR/$DYLIB_NAME" "$DOWNLOAD_URL"

if [ ! -f "$INSTALL_DIR/$DYLIB_NAME" ]; then
    echo "[-] Download failed!"
    exit 1
fi

echo "[+] Installed to $INSTALL_DIR/$DYLIB_NAME"
echo "[+] Run start.sh to launch with injection"
