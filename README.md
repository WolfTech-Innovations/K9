# K9 Desktop Environment

K9 is a lightweight and modern X11-based desktop environment, designed for a sleek and efficient workflow. It features rounded window decorations, a system panel, blur effects, and smooth window management.

## Features
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
git clone https://github.com/WolfTech-Innovations/K9.git
cd K9-DE
gcc -o k9 main.c -lX11 -lXext -lXrender -lXfixes -lXcomposite -lXcursor -lXft -lm
./k9
```

## Keybindings
- `Super+I`: Open system info popup
- `Super+M`: Open menu

## Contributing
Contributions are welcome! Feel free to submit a pull request or report issues.

## License
GPL License

