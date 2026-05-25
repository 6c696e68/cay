#!/bin/bash
# GitHub: https://github.com/tctvn/cay
# Lệnh cài đặt: wget -qO- https://raw.githubusercontent.com/tctvn/cay/main/scripts/install-mac.sh | bash

# CayIME - macOS Automatic Installer Script
# This script downloads the pre-built CayIME macOS app from GitHub and installs it.

set -e

# Colors for terminal output
GREEN='\032[0;32m'
CYAN='\032[0;36m'
YELLOW='\032[1;33m'
RED='\032[0;31m'
NC='\032[0m' # No Color

echo -e "${CYAN}================================================${NC}"
echo -e "${CYAN}   CayIME macOS Automatic Installer             ${NC}"
echo -e "${CYAN}================================================${NC}"

REPO="tctvn/cay"
ASSET_NAME="cay-mac.zip"
DOWNLOAD_URL="https://github.com/${REPO}/releases/latest/download/${ASSET_NAME}"
INSTALL_DIR="/Applications"
APP_NAME="cay.app"
APP_PATH="${INSTALL_DIR}/${APP_NAME}"

# Step 1: Download pre-built binary
echo -e "\n${YELLOW}[1/3] Downloading latest pre-built CayIME for macOS...${NC}"
WORK_DIR=$(mktemp -d)
cd "$WORK_DIR"

if ! curl -sL "$DOWNLOAD_URL" -o "$ASSET_NAME"; then
    echo -e "${RED}Failed to download ${ASSET_NAME}. Please make sure the release exists on GitHub.${NC}"
    exit 1
fi

# Step 2: Extract and install
echo -e "\n${YELLOW}[2/3] Extracting and installing to /Applications...${NC}"
unzip -q "$ASSET_NAME"

# Close app if running
echo -e "\n${YELLOW}[2.5/3] Closing existing app...${NC}"
killall cay 2>/dev/null || true
sleep 1
killall -9 cay 2>/dev/null || true

# Copy to Applications (may require sudo if permissions are tight, but usually user can copy)
if [ -d "$APP_PATH" ]; then
    rm -rf "$APP_PATH"
fi
cp -R "$APP_NAME" "$INSTALL_DIR/"

# Setup auto-start exactly how the app does it
echo -e "\n${YELLOW}[3/3] Setting up automatic startup (LaunchAgent)...${NC}"
BUNDLE_ID="com.tctvn.cay"
PLIST_DIR="$HOME/Library/LaunchAgents"
PLIST_PATH="$PLIST_DIR/${BUNDLE_ID}.plist"
EXEC_PATH="/Applications/cay.app/Contents/MacOS/cay"

mkdir -p "$PLIST_DIR"
cat > "$PLIST_PATH" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>$BUNDLE_ID</string>
    <key>ProgramArguments</key>
    <array>
        <string>$EXEC_PATH</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
</dict>
</plist>
EOF

# Remove Apple quarantine to avoid "App is damaged" error since it's not signed by App Store
echo -e "\n${YELLOW}[4/4] Clearing quarantine flags and launching...${NC}"
xattr -cr "$APP_PATH"
# Cleanup
cd ~
rm -rf "$WORK_DIR"

# Launch
open "$APP_PATH"

# Step 3: Final Instructions
echo -e "\n${GREEN}================================================${NC}"
echo -e "${GREEN}   Installation Successful!                     ${NC}"
echo -e "${GREEN}================================================${NC}"
echo -e "\n${CYAN}CayIME is now installed in your Applications folder and running.${NC}"
echo -e "You might need to grant it Accessibility permissions in System Settings if it's your first time."
echo -e "Enjoy typing in Vietnamese!\n"
