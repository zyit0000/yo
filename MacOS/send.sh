#!/bin/bash
# wadiwad macOS script sender — connects to executor TCP server
HOST="127.0.0.1"
PORT=5555

echo "========================================="
echo "  wadiwad macOS executor"
echo "  Commands: attach, status"
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

while true; do
    printf ">>> "
    read -r line
    if [ -z "$line" ]; then continue; fi
    echo "$line" | nc -w 1 "$HOST" "$PORT" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "[!] Failed to send. Server may be down."
    fi
done
