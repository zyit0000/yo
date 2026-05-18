#!/bin/bash
# wadiwad macOS start — launches Roblox with dylib injection + opens executor REPL
set -e

# ---- config ----
REPO="zyit0000/yo"
DYLIB_NAME="executor.dylib"
INSTALL_DIR="$HOME/.wadiwad"
ROBLOX_APP="/Applications/Roblox.app/Contents/MacOS/RobloxPlayer"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# -----------------

echo "[*] wadiwad macOS launcher"

# check/download dylib
if [ ! -f "$INSTALL_DIR/$DYLIB_NAME" ]; then
    echo "[*] Dylib not found, downloading latest..."
    mkdir -p "$INSTALL_DIR"
    curl -L -o "$INSTALL_DIR/$DYLIB_NAME" \
        "https://github.com/$REPO/releases/latest/download/$DYLIB_NAME"
    if [ ! -f "$INSTALL_DIR/$DYLIB_NAME" ]; then
        echo "[-] Download failed! Run setup.sh first."
        exit 1
    fi
    echo "[+] Downloaded dylib"
fi

# find Roblox binary
if [ ! -f "$ROBLOX_APP" ]; then
    ROBLOX_APP=$(find /Applications -name "RobloxPlayer" -type f 2>/dev/null | head -1)
    if [ -z "$ROBLOX_APP" ]; then
        echo "[-] RobloxPlayer not found"
        exit 1
    fi
fi

echo "[+] Roblox: $ROBLOX_APP"
echo "[+] Dylib:  $INSTALL_DIR/$DYLIB_NAME"

# strip library validation from Roblox so it accepts our dylib
echo "[*] Stripping Roblox code signature (library validation)..."
codesign --remove-signature "$ROBLOX_APP" 2>/dev/null || true
codesign -f -s - "$ROBLOX_APP" 2>/dev/null || true
echo "[+] Signature stripped"

# open a NEW terminal window with the script sender REPL
SEND_SCRIPT="$INSTALL_DIR/send.sh"
if [ ! -f "$SEND_SCRIPT" ]; then
    SEND_SCRIPT="$SCRIPT_DIR/send.sh"
fi
if [ -f "$SEND_SCRIPT" ]; then
    chmod +x "$SEND_SCRIPT"
    osascript -e "tell application \"Terminal\" to do script \"'$SEND_SCRIPT'\"" &
    echo "[+] Opened executor REPL in new Terminal window"
else
    echo "[!] send.sh not found — run setup.sh first"
fi

# launch Roblox with dylib injection (this terminal shows dylib logs)
echo "[*] Launching Roblox with dylib injection..."
echo "--- Roblox + dylib output below ---"
DYLD_INSERT_LIBRARIES="$INSTALL_DIR/$DYLIB_NAME" "$ROBLOX_APP"
