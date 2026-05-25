#!/bin/bash
# GitHub: https://github.com/tctvn/cay
# Lệnh cài đặt: wget -qO- https://raw.githubusercontent.com/tctvn/cay/main/scripts/install-fcitx5.sh | bash

# CayIME - Fcitx5 Automatic Installer Script
# This script downloads the pre-built Fcitx5 plugin from GitHub and installs it.

set -e

# Colors for terminal output
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${CYAN}================================================${NC}"
echo -e "${CYAN}   CayIME Fcitx5 Plugin Automatic Installer     ${NC}"
echo -e "${CYAN}================================================${NC}"

REPO="tctvn/cay"
ASSET_NAME="cayime-fcitx5-linux.tar.gz"
DOWNLOAD_URL="https://github.com/${REPO}/releases/latest/download/${ASSET_NAME}"

# Step 1: Download pre-built binary
echo -e "\n${YELLOW}[1/3] Downloading latest pre-built plugin from GitHub...${NC}"
WORK_DIR=$(mktemp -d)
cd "$WORK_DIR"

if ! wget -q --show-progress "$DOWNLOAD_URL"; then
    echo -e "${RED}Failed to download ${ASSET_NAME}. Please make sure the 'nightly' release exists on GitHub.${NC}"
    exit 1
fi

# Step 2: Extract and install
echo -e "\n${YELLOW}[2/3] Extracting and installing plugin to system (/usr)...${NC}"
tar -xzf "$ASSET_NAME"

# Kill fcitx5 before installing to release file locks
echo -e "Stopping Fcitx5 to release file locks..."
killall fcitx5 2>/dev/null || true
sleep 1
killall -9 fcitx5 2>/dev/null || true
sleep 1

echo "We need your password (sudo) to copy files to /usr/lib and /usr/share."
# Use -f to force overwrite
sudo cp -rf usr/* /usr/

# Cleanup
cd ~
rm -rf "$WORK_DIR"

# Step 3: Auto-configure Fcitx5 Profile (Zero-Click Magic)
echo -e "\n${YELLOW}[3/3] Automatically configuring Fcitx5...${NC}"

PROFILE="$HOME/.config/fcitx5/profile"
mkdir -p "$HOME/.config/fcitx5"

# Nếu user chưa từng có profile (cài fcitx5 mới tinh), tạo profile mặc định
if [ ! -f "$PROFILE" ]; then
cat <<EOF > "$PROFILE"
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=cayime

[Groups/0/Items/0]
Name=cayime
Layout=

[Groups/0/Items/1]
Name=keyboard-us
Layout=

[GroupOrder]
0=Default
EOF
else
    # Nếu profile đã tồn tại, kiểm tra xem CayIME đã được add chưa
    if ! grep -qi "Name=cayime" "$PROFILE"; then
        echo -e "Injecting CayIME into existing Fcitx5 profile..."
        
        # Tìm index lớn nhất hiện tại trong danh sách Items của Group 0
        MAX_ITEM=$(grep -o '\[Groups/0/Items/[0-9]*\]' "$PROFILE" | cut -d '/' -f 3 | tr -d ']' | sort -n | tail -1)
        
        if [ -z "$MAX_ITEM" ]; then
            NEXT_ITEM=1
        else
            NEXT_ITEM=$((MAX_ITEM + 1))
        fi
        
        # Đẩy bộ gõ đang ở vị trí 0 xuống cuối (NEXT_ITEM) để nhường chỗ cho CayIME
        sed -i "s/\[Groups\/0\/Items\/0\]/\[Groups\/0\/Items\/$NEXT_ITEM\]/g" "$PROFILE"
        
        # Bơm cấu hình CayIME vào vị trí 0 (Ưu tiên cao nhất)
        echo "" >> "$PROFILE"
        echo "[Groups/0/Items/0]" >> "$PROFILE"
        echo "Name=cayime" >> "$PROFILE"
        echo "Layout=" >> "$PROFILE"
    fi
    
    # Luôn luôn ép cập nhật DefaultIM thành cayime để gõ tiếng Việt ngay lập tức
    sed -i 's/^DefaultIM=.*/DefaultIM=cayime/' "$PROFILE"
fi

# Đặt Fcitx5 làm bộ gõ mặc định của hệ thống (giúp tự khởi động cùng Ubuntu/Debian)
echo -e "\n${YELLOW}[4/4] Setting Fcitx5 as default system input method (Autostart)...${NC}"
if command -v im-config >/dev/null 2>&1; then
    im-config -n fcitx5
else
    # Fallback nếu không có im-config
    echo "run_im fcitx5" > ~/.xinputrc
fi

# Ép tự khởi động Fcitx5 bằng desktop entry (sửa lỗi GNOME Wayland)
mkdir -p ~/.config/autostart
cp /usr/share/applications/org.fcitx.Fcitx5.desktop ~/.config/autostart/ 2>/dev/null || true

# Khởi động lại Fcitx5 trong nền để nạp cấu hình mới (tắt tiếng/output)
nohup fcitx5 -r -d >/dev/null 2>&1 &
disown

echo -e "\n${GREEN}================================================${NC}"
echo -e "${GREEN}   Installation Successful!                     ${NC}"
echo -e "${GREEN}================================================${NC}"
echo -e "${CYAN}CayIME đã được tự động thêm vào Fcitx5.${NC}"
echo -e "Hãy nhấn Ctrl+Space (hoặc phím tắt chuyển ngôn ngữ của bạn) và tận hưởng trải nghiệm gõ bay bổng!\n"
