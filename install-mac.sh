#!/bin/bash

# Cay - macOS Automatic Installer
# Tải bản build sẵn từ GitHub Releases và cài vào /Applications.

set -e

# Màu terminal — dùng printf -e an toàn ở mọi shell. ESC là \033 (KHÔNG phải \032).
GREEN=$'\033[0;32m'
CYAN=$'\033[0;36m'
YELLOW=$'\033[1;33m'
RED=$'\033[0;31m'
NC=$'\033[0m'

echo "${CYAN}================================================${NC}"
echo "${CYAN}   Cay macOS Automatic Installer                ${NC}"
echo "${CYAN}================================================${NC}"

REPO="tctvn/cay"
ASSET_NAME="cay-mac.zip"
DOWNLOAD_URL="https://github.com/${REPO}/releases/latest/download/${ASSET_NAME}"
INSTALL_DIR="/Applications"
APP_NAME="cay.app"
APP_PATH="${INSTALL_DIR}/${APP_NAME}"
BUNDLE_ID="com.tctvn.cay"

# 1. Tải bản build sẵn
echo
echo "${YELLOW}[1/4] Đang tải Cay phiên bản mới nhất cho macOS...${NC}"
WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT
cd "$WORK_DIR"

if ! curl -fsSL "$DOWNLOAD_URL" -o "$ASSET_NAME"; then
    echo "${RED}Tải ${ASSET_NAME} thất bại. Kiểm tra release tại github.com/${REPO}/releases.${NC}"
    exit 1
fi

# 2. Giải nén & cài
echo
echo "${YELLOW}[2/4] Giải nén và cài vào /Applications...${NC}"
unzip -q "$ASSET_NAME"

if [ ! -d "$APP_NAME" ]; then
    echo "${RED}Không tìm thấy ${APP_NAME} trong ${ASSET_NAME}.${NC}"
    exit 1
fi

# Đóng app nếu đang chạy
killall cay 2>/dev/null || true

if [ -d "$APP_PATH" ]; then
    rm -rf "$APP_PATH"
fi

if ! cp -R "$APP_NAME" "$INSTALL_DIR/" 2>/dev/null; then
    echo "${YELLOW}Cần quyền sudo để ghi vào /Applications...${NC}"
    sudo cp -R "$APP_NAME" "$INSTALL_DIR/"
fi

# 3. Bỏ cờ quarantine của Gatekeeper (binary chưa ký App Store)
echo
echo "${YELLOW}[3/4] Bỏ cờ quarantine...${NC}"
xattr -cr "$APP_PATH" 2>/dev/null || sudo xattr -cr "$APP_PATH"

# 4. LaunchAgent tự khởi động cùng macOS — đồng bộ với menu trong app
echo
echo "${YELLOW}[4/4] Cấu hình tự khởi động (LaunchAgent)...${NC}"
PLIST_DIR="$HOME/Library/LaunchAgents"
PLIST_PATH="$PLIST_DIR/${BUNDLE_ID}.plist"
EXEC_PATH="${APP_PATH}/Contents/MacOS/cay"

mkdir -p "$PLIST_DIR"
cat > "$PLIST_PATH" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>${BUNDLE_ID}</string>
    <key>ProgramArguments</key>
    <array>
        <string>${EXEC_PATH}</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
</dict>
</plist>
EOF

# Khởi chạy app
open "$APP_PATH"

echo
echo "${GREEN}================================================${NC}"
echo "${GREEN}   Cài đặt thành công!                          ${NC}"
echo "${GREEN}================================================${NC}"
echo
echo "${CYAN}Cay đã được cài vào /Applications và đang chạy.${NC}"
echo "Lần đầu chạy, hãy cấp quyền Accessibility:"
echo "  System Settings → Privacy & Security → Accessibility → bật ${YELLOW}cay${NC}"
echo "Sau khi cấp quyền, Cay sẽ tự nhận và bắt đầu hoạt động — không cần thoát app."
echo
