gcc main.c -o k9-session -lX11
# Define the target file
TARGET_FILE="/usr/share/xsessions/k9.desktop"

# Ensure the script is run as root
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi

# Create the desktop entry
cat <<EOF > "$TARGET_FILE"
[Desktop Entry]
Name=K9 Desktop
Comment=A lightweight X11-based desktop environment
Exec=/bin/k9-session
Type=Application
X-LightDM-DesktopName=K9
EOF

# Set proper permissions
chmod 644 "$TARGET_FILE"

# Notify the user
echo "K9 desktop session file created at $TARGET_FILE" 
sudo cp k9* /bin/
alias k9=/bin/k9-session
