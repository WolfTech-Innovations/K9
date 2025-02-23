import os
import sys
import subprocess
import logging
from Xlib import X, display

# Set up logging
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')

# Paths for session files
XSESSIONS_PATH = "/usr/share/xsessions/K9.desktop"
SESSION_SCRIPT_PATH = "/usr/bin/k9-session"
XDG_CONFIG_PATH = "/etc/xdg/K9/"

# Ensure the required session files exist
def setup_desktop_environment():
    if not os.path.exists(XSESSIONS_PATH):
        logging.info("Creating X11 session file for K9 DE.")
        with open(XSESSIONS_PATH, "w") as f:
            f.write("""[Desktop Entry]
Name=K9 Desktop
Comment=K9 Linux Desktop Environment
Exec=/usr/bin/k9-session
Type=Application
DesktopNames=K9
""")
        os.chmod(XSESSIONS_PATH, 0o644)
    
    if not os.path.exists(SESSION_SCRIPT_PATH):
        logging.info("Creating K9 session startup script.")
        with open(SESSION_SCRIPT_PATH, "w") as f:
            f.write("""#!/bin/bash
# Start K9 Desktop Environment
exec k9-window-manager
""")
        os.chmod(SESSION_SCRIPT_PATH, 0o755)
    
    if not os.path.exists(XDG_CONFIG_PATH):
        logging.info("Setting up XDG configuration for K9 DE.")
        os.makedirs(XDG_CONFIG_PATH, exist_ok=True)
    
setup_desktop_environment()

# Handle Super+I to show system info
def show_system_info():
    logging.info("Displaying system information popup.")
    os.system("zenity --info --text='K9 Desktop Environment (Detected as DE)' --title='System Info'")

display = display.Display()
root = display.screen().root
root.change_attributes(event_mask=X.KeyPressMask)

logging.info("K9 Desktop Environment setup complete.")
