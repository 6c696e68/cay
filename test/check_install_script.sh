#!/bin/bash
# Smoke check tĩnh: install-mac.sh phải hợp lệ cú pháp và dùng ESC \033 (không phải \032).
# KHÔNG chạy script — script thực hiện cài đặt thật.
set -e
SCRIPT="$(dirname "$0")/../install-mac.sh"

echo "== syntax check =="
bash -n "$SCRIPT"
echo "OK"

echo
echo "== ESC literal check =="
if grep -nE $'\\\\032\\[' "$SCRIPT" >/dev/null 2>&1; then
    echo "FAIL: phát hiện ESC sai \\032 (Ctrl-Z) trong $SCRIPT"
    exit 1
fi
if ! grep -nE '\\033\[' "$SCRIPT" >/dev/null 2>&1; then
    echo "FAIL: không tìm thấy ESC \\033 — màu sẽ không hoạt động"
    exit 1
fi
echo "OK: chỉ dùng \\033"

echo
echo "== executable bit =="
test -x "$SCRIPT" && echo "OK"
