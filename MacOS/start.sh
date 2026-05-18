#!/bin/bash
# wadiwad macOS start — injects dylib via DYLD_INSERT_LIBRARIES then connects
set -e

# ---- config ----
REPO="zyit0000/yo"
DYLIB_NAME="executor.dylib"
INSTALL_DIR="$HOME/.wadiwad"
ROBLOX_APP="/Applications/Roblox.app/Contents/MacOS/RobloxPlayer"
HOST="127.0.0.1"
PORT=5555
# -----------------

echo "[*] wadiwad macOS launcher"

# check/download dylib
if [ ! -f "$INSTALL_DIR/$DYLIB_NAME" ]; then
    echo "[*] Dylib not found, downloading latest..."
    mkdir -p "$INSTALL_DIR"
    DOWNLOAD_URL="https://github.com/$REPO/releases/latest/download/$DYLIB_NAME"
    curl -L -o "$INSTALL_DIR/$DYLIB_NAME" "$DOWNLOAD_URL"
    if [ ! -f "$INSTALL_DIR/$DYLIB_NAME" ]; then
        echo "[-] Download failed! Run setup.sh first."
        exit 1
    fi
    echo "[+] Downloaded dylib"
fi

# find Roblox binary
if [ ! -f "$ROBLOX_APP" ]; then
    # try alternate paths
    ROBLOX_APP=$(find /Applications -name "RobloxPlayer" -type f 2>/dev/null | head -1)
    if [ -z "$ROBLOX_APP" ]; then
        echo "[-] RobloxPlayer not found in /Applications"
        echo "[-] Install Roblox first"
        exit 1
    fi
fi

echo "[+] Roblox: $ROBLOX_APP"
echo "[+] Dylib: $INSTALL_DIR/$DYLIB_NAME"
echo "[*] Launching Roblox with dylib injection..."

# launch Roblox with DYLD_INSERT_LIBRARIES
DYLD_INSERT_LIBRARIES="$INSTALL_DIR/$DYLIB_NAME" "$ROBLOX_APP" &
ROBLOX_PID=$!
echo "[+] Roblox PID: $ROBLOX_PID"

# wait for TCP server to come up
echo "[*] Waiting for executor server on $HOST:$PORT..."
for i in $(seq 1 30); do
    if nc -z "$HOST" "$PORT" 2>/dev/null; then
        echo "[+] Server is up!"
        break
    fi
    if [ $i -eq 30 ]; then
        echo "[-] Timed out waiting for server"
        exit 1
    fi
    sleep 1
done

# interactive script REPL
echo ""
echo "========================================="
echo "  wadiwad macOS executor — connected"
echo "  Type Lua scripts, press Enter to run"
echo "  Ctrl+C to quit"
echo "========================================="
echo ""

while true; do
    printf ">>> "
    read -r line
    if [ -z "$line" ]; then continue; fi
    echo "$line" | nc -w 1 "$HOST" "$PORT" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "[!] Failed to send. Server may have crashed."
    fi
done
