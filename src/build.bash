gcc main.c -o k9-session -lX11
# Ensure the script is run as root
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi
# Set proper permissions
chmod 644 "$TARGET_FILE"

# Notify the user
echo "K9 desktop session file created at $TARGET_FILE" 
sudo cp k9* /bin/
alias k9=/bin/k9-session
