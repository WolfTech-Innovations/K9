# K9 Desktop Environment

K9 is a lightweight and modern X11-based desktop environment inspired by macOS, designed for a sleek and efficient workflow. It features rounded window decorations, a system panel, blur effects, and smooth window management.

## Features
- **MacOS-inspired UI**: Rounded window decorations, title bars, and traffic light buttons.
- **X11 Window Management**: Manages windows with a custom compositor.
- **Blurred Background Effects**: Uses XRender for a smooth, blurred look.
- **System Menu**: Accessible via a right-click or Super key.
- **System Info Popup**: Press `Super+I` to view system details, including K9 registration as a DE.
- **Lightweight**: Minimal dependencies, optimized for performance.

## Installation
### Dependencies
Ensure you have the following installed:
```sh
sudo apt install libx11-dev libxrender-dev libxext-dev libxrandr-dev libxft-dev libxcomposite-dev
```

### Build and Run
```sh
git clone https://github.com/yourusername/K9-DE.git
cd K9-DE
make
./k9
```

## Registering K9 as a Desktop Environment
To register K9 as a DE with X11, add the following to `/usr/share/xsessions/k9.desktop`:
```
[Desktop Entry]
Name=K9 Desktop
Comment=A lightweight X11-based desktop environment
Exec=/path/to/k9
Type=Application
X-LightDM-DesktopName=K9
```

## Keybindings
- `Super+I`: Open system info popup
- `Super+M`: Open menu

## Contributing
Contributions are welcome! Feel free to submit a pull request or report issues.

## License
GPL License

