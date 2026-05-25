#!/bin/bash
# GitHub: https://github.com/tctvn/cay
# Lệnh cài đặt: wget -qO- https://raw.githubusercontent.com/tctvn/cay/main/scripts/uninstall-fcitx5.sh | bash

# CayIME - Fcitx5 Uninstaller
# Script này gỡ bỏ hoàn toàn CayIME khỏi Fcitx5 (cả thư mục hệ thống lẫn local)

set -e

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${CYAN}================================================${NC}"
echo -e "${CYAN}        CayIME Fcitx5 Uninstaller               ${NC}"
echo -e "${CYAN}================================================${NC}"

# Tắt fcitx5 để tránh file bị khóa hoặc lock
echo -e "\n${YELLOW}[1/4] Đang dừng tiến trình Fcitx5...${NC}"
killall fcitx5 2>/dev/null || true
sleep 1
killall -9 fcitx5 2>/dev/null || true
sleep 1

# Xóa file trên hệ thống (/usr)
echo -e "\n${YELLOW}[2/4] Xóa các file plugin hệ thống (cần quyền sudo)...${NC}"
echo "Vui lòng nhập mật khẩu nếu được yêu cầu:"

sudo rm -f /usr/lib/fcitx5/cayime-fcitx5.so
sudo rm -f /usr/lib/x86_64-linux-gnu/fcitx5/cayime-fcitx5.so
sudo rm -f /usr/share/fcitx5/addon/cayime.conf
sudo rm -f /usr/share/fcitx5/inputmethod/cayime-im.conf
sudo rm -f /usr/share/fcitx5/inputmethod/cayime.conf
sudo rm -f /usr/share/icons/hicolor/scalable/apps/cayime.svg

# Xóa file local của user (nếu có)
echo -e "\n${YELLOW}[3/4] Dọn dẹp cache và file local...${NC}"
rm -f ~/.local/lib/fcitx5/cayime-fcitx5.so
rm -f ~/.local/lib/x86_64-linux-gnu/fcitx5/cayime-fcitx5.so
rm -f ~/.local/share/fcitx5/addon/cayime.conf
rm -f ~/.local/share/fcitx5/inputmethod/cayime-im.conf
rm -f ~/.local/share/fcitx5/inputmethod/cayime.conf
rm -rf ~/.config/fcitx5/cayime 2>/dev/null || true

# Khôi phục DefaultIM trong Fcitx5 profile về tiếng Anh nếu đang để CayIME
PROFILE="$HOME/.config/fcitx5/profile"
if [ -f "$PROFILE" ]; then
    sed -i 's/^DefaultIM=cayime/DefaultIM=keyboard-us/' "$PROFILE"
fi

# Xóa cache để Fcitx5 quét lại danh sách Addon
rm -rf ~/.cache/fcitx5

echo -e "\n${YELLOW}[4/4] Khởi động lại Fcitx5...${NC}"
nohup fcitx5 -d -r >/dev/null 2>&1 &
disown

echo -e "\n${GREEN}================================================${NC}"
echo -e "${GREEN}   Gỡ cài đặt thành công! CayIME đã bị xóa.     ${NC}"
echo -e "${GREEN}================================================${NC}"
