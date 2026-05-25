#!/bin/bash
# GitHub: https://github.com/tctvn/cay
# Lệnh cài đặt: wget -qO- https://raw.githubusercontent.com/tctvn/cay/main/scripts/install-local-test.sh | bash

# CayIME - Local Fcitx5 Plugin Installer for Testing
# This script builds the Fcitx5 plugin from local source and installs it using sudo.

set -e

# Colors for terminal output
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${CYAN}================================================${NC}"
echo -e "${CYAN}   CayIME Fcitx5 Plugin Local Test Installer    ${NC}"
echo -e "${CYAN}================================================${NC}"

# Go to the project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
cd "$SCRIPT_DIR/.."

# Ensure we are in the project root
if [ ! -f "CMakeLists.txt" ] || [ ! -d "src/platform/fcitx5" ]; then
    echo -e "${RED}Lỗi: Không tìm thấy thư mục gốc của dự án (thiếu CMakeLists.txt).${NC}"
    exit 1
fi

# Step 1: Kill Fcitx5 completely to release file locks
echo -e "\n${YELLOW}[1/3] Stopping Fcitx5...${NC}"
killall fcitx5 2>/dev/null || true
sleep 1
# Force kill if it's still hanging (prevents stuck keys if killed abruptly)
killall -9 fcitx5 2>/dev/null || true
sleep 1

# Step 2: Install to system using sudo
echo -e "\n${YELLOW}[2/3] Installing plugin to system (/usr)...${NC}"

# Find build directory
BUILD_DIR=""
if [ -d "build_local" ] && [ -f "build_local/Makefile" ]; then BUILD_DIR="build_local"; fi
if [ -d "build_system" ] && [ -f "build_system/Makefile" ]; then BUILD_DIR="build_system"; fi
if [ -d "build" ] && [ -f "build/Makefile" ]; then BUILD_DIR="build"; fi

if [ -z "$BUILD_DIR" ]; then
    echo -e "${RED}Lỗi: Không tìm thấy thư mục build (build_local, build_system, hoặc build). Vui lòng tự build project trước!${NC}"
    exit 1
fi

echo "We need your password (sudo) to install files to /usr/lib and /usr/share."
# Only install, do NOT build
sudo cmake --install "$BUILD_DIR"

# Nếu Fcitx5 trên Ubuntu/Debian yêu cầu chuẩn kiến trúc, ta copy dự phòng sang x86_64-linux-gnu
if [ -d "/usr/lib/x86_64-linux-gnu/fcitx5" ]; then
    sudo cp -f "$BUILD_DIR/src/platform/fcitx5/cayime-fcitx5.so" /usr/lib/x86_64-linux-gnu/fcitx5/ 2>/dev/null || true
fi

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

# Khởi động lại Fcitx5 trong nền để nạp cấu hình và module mới
nohup fcitx5 -d -r >/dev/null 2>&1 &
disown

echo -e "\n${GREEN}================================================${NC}"
echo -e "${GREEN}   Local Installation Successful!               ${NC}"
echo -e "${GREEN}================================================${NC}"
echo -e "${CYAN}CayIME đã được build lại và tự động cập nhật vào Fcitx5.${NC}"
echo -e "Hãy nhấn Ctrl+Space (hoặc phím tắt chuyển ngôn ngữ của bạn) và tận hưởng trải nghiệm gõ bay bổng!\n"
