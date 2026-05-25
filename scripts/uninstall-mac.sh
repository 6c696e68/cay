#!/bin/bash
# GitHub: https://github.com/tctvn/cay
# Lệnh cài đặt: wget -qO- https://raw.githubusercontent.com/tctvn/cay/main/scripts/uninstall-mac.sh | bash

# CayIME - macOS Automatic Uninstaller Script

set -e

# Colors for terminal output
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${CYAN}================================================${NC}"
echo -e "${CYAN}   CayIME macOS Automatic Uninstaller           ${NC}"
echo -e "${CYAN}================================================${NC}"

# Step 1: Kill the app
echo -e "\n${YELLOW}[1/3] Closing CayIME...${NC}"
killall cay 2>/dev/null || true
sleep 1
killall -9 cay 2>/dev/null || true

# Step 2: Remove the LaunchAgent
echo -e "\n${YELLOW}[2/3] Removing automatic startup...${NC}"
BUNDLE_ID="com.tctvn.cay"
PLIST_PATH="$HOME/Library/LaunchAgents/${BUNDLE_ID}.plist"

if [ -f "$PLIST_PATH" ]; then
    launchctl unload "$PLIST_PATH" 2>/dev/null || true
    rm -f "$PLIST_PATH"
fi

# Step 3: Remove the application
echo -e "\n${YELLOW}[3/3] Removing application files...${NC}"
INSTALL_DIR="/Applications"
APP_NAME="cay.app"
APP_PATH="${INSTALL_DIR}/${APP_NAME}"

if [ -d "$APP_PATH" ]; then
    rm -rf "$APP_PATH"
fi

echo -e "\n${GREEN}================================================${NC}"
echo -e "${GREEN}   Uninstallation Successful!                   ${NC}"
echo -e "${GREEN}================================================${NC}"
echo -e "\n${CYAN}CayIME has been completely removed from your Mac.${NC}\n"
