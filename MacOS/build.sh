#!/bin/bash
# Build the wadiwad executor dylib for macOS
# Targets Intel x86_64, macOS 10.13+ (High Sierra)
# Requires Xcode command line tools: xcode-select --install

set -e

echo "[*] Building executor.dylib (x86_64, macOS 10.13+)..."
clang++ -std=c++17 -O2 -shared -dynamiclib \
    -mmacosx-version-min=10.13 \
    -arch x86_64 \
    -framework CoreFoundation \
    -framework Security \
    -o executor.dylib \
    executor.cpp

if [ $? -eq 0 ]; then
    echo "[+] Built: executor.dylib"
    file executor.dylib
    chmod +x setup.sh start.sh
    echo "[+] Done. Run setup.sh on target mac, then start.sh to inject."
else
    echo "[-] Build failed"
    exit 1
fi
