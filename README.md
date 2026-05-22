# K9 Desktop Environment (Wayland)

K9 is a modern, polished Wayland desktop environment built with `wlroots` and GTK. It provides a unified, singular user experience with a sleek control center and dock.

## Features
- **Wayland Native**: High performance and security.
- **Unified Shell**: GTK-based panel and dock with a modern control center.
- **Polished UI**: Rounded corners, Tokyonight theme, and intuitive widgets.
- **Interactive**: Support for moving, resizing, and focusing windows.

## Installation
### Dependencies
```sh
sudo apt update
sudo apt install -y libwlroots-dev libwayland-dev wayland-protocols \
    pkg-config libxkbcommon-dev libpixman-1-dev libgles2-mesa-dev \
    libinput-dev libgbm-dev libudev-dev libgtk-3-dev libgtk-layer-shell-dev
```

### Build and Install
```sh
cd src
bash build.bash
sudo cp k9-compositor k9-shell /usr/local/bin/
```

### Running
To start the K9 session:
```sh
k9-compositor
```
*Tip: Use Alt+Escape to exit the compositor.*

## License
GPL License
