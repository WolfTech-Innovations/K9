#!/bin/bash
set -e

# Compile Compositor
gcc -o k9-compositor main.c \
    $(pkg-config --cflags --libs wlroots wayland-server xkbcommon) \
    -DWLR_USE_UNSTABLE

# Compile Shell
gcc -o k9-shell shell.c \
    $(pkg-config --cflags --libs gtk+-3.0 gtk-layer-shell-0)

# Install
if [[ $EUID -eq 0 ]]; then
    cp k9-compositor k9-shell /usr/local/bin/
    echo "K9 components installed to /usr/local/bin/"
else
    echo "Run with sudo to install to /usr/local/bin/"
    echo "Local binaries created in src/ directory"
fi
