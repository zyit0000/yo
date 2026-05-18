#!/bin/bash
# wadiwad macOS script sender — connects to executor TCP server
REPO="zyit0000/yo"
HOST="127.0.0.1"
PORT=5555
INSTALL_DIR="$HOME/.wadiwad"
SCRIPT_DIR="$(cd "$(dirname "$0")" 2>/dev/null && pwd || echo "$INSTALL_DIR")"

echo "========================================="
echo "  wadiwad macOS executor"
echo "  Commands:"
echo "    attach     — connect to Roblox VM"
echo "    status     — show connection info"
echo "    run(unc)   — run UNC environment test"
echo "    run(file)  — run a .lua file"
echo "  Or type Lua scripts to execute"
echo "  Ctrl+C to quit"
echo "========================================="
echo ""

# wait for server
echo "[*] Waiting for server on $HOST:$PORT..."
for i in $(seq 1 60); do
    if nc -z "$HOST" "$PORT" 2>/dev/null; then
        echo "[+] Connected!"
        break
    fi
    if [ $i -eq 60 ]; then
        echo "[-] Timed out. Is Roblox running with the dylib?"
        exit 1
    fi
    sleep 1
done
echo ""

# download lua.lua (UNC test) if not present
UNC_FILE="$INSTALL_DIR/lua.lua"
if [ ! -f "$UNC_FILE" ]; then
    echo "[*] Downloading UNC test script..."
    curl -sL -o "$UNC_FILE" "https://raw.githubusercontent.com/$REPO/main/lua.lua" 2>/dev/null || true
fi

while true; do
    printf ">>> "
    read -r line
    if [ -z "$line" ]; then continue; fi

    # run(unc) — send the UNC test script
    if [ "$line" = "run(unc)" ]; then
        if [ -f "$UNC_FILE" ]; then
            echo "[*] Running UNC test (lua.lua)..."
            cat "$UNC_FILE" | nc -w 2 "$HOST" "$PORT" 2>/dev/null
            if [ $? -eq 0 ]; then
                echo "[+] UNC test sent"
            else
                echo "[!] Failed to send"
            fi
        else
            echo "[!] lua.lua not found at $UNC_FILE"
            echo "[!] Download it: curl -L -o $UNC_FILE https://raw.githubusercontent.com/$REPO/main/lua.lua"
        fi
        continue
    fi

    # run(file) — prompt for file path and send it
    if [ "$line" = "run(file)" ]; then
        printf "File path: "
        read -r filepath
        if [ -f "$filepath" ]; then
            echo "[*] Running $filepath..."
            cat "$filepath" | nc -w 2 "$HOST" "$PORT" 2>/dev/null
            echo "[+] Sent"
        else
            echo "[!] File not found: $filepath"
        fi
        continue
    fi

    # regular command/script — send directly
    echo "$line" | nc -w 1 "$HOST" "$PORT" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "[!] Failed to send. Server may be down."
    fi
done
