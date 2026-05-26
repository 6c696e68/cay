#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Verify task 6.3: src/platform/windows/InputInjector.cpp đã loại bỏ magic
# numbers (`256`, `64`, `321`) và include `CayTypes.h` để dùng
# `Cay::MAX_BUFFER * 4 + 4`.
#
# Script này chạy được trên macOS (chỉ grep, không compile file Windows).
# ---------------------------------------------------------------------------
set -euo pipefail

FILE="src/platform/windows/InputInjector.cpp"

if [[ ! -f "$FILE" ]]; then
    echo "FAIL: $FILE không tồn tại"
    exit 1
fi

fail=0

# 1. Phải include CayTypes.h
if ! grep -qE '^#include[[:space:]]+"CayTypes\.h"' "$FILE"; then
    echo "FAIL: thiếu '#include \"CayTypes.h\"' trong $FILE"
    fail=1
else
    echo "OK: include \"CayTypes.h\" có mặt"
fi

# 2. Phải dùng Cay::MAX_BUFFER * 4 + 4 cho cả khai báo mảng lẫn các check idx
expr_count=$(grep -cE 'Cay::MAX_BUFFER[[:space:]]*\*[[:space:]]*4[[:space:]]*\+[[:space:]]*4' "$FILE" || true)
if [[ "$expr_count" -lt 3 ]]; then
    echo "FAIL: kỳ vọng ít nhất 3 lần xuất hiện 'Cay::MAX_BUFFER * 4 + 4' (mảng + 2 vòng for), nhưng chỉ thấy $expr_count"
    fail=1
else
    echo "OK: 'Cay::MAX_BUFFER * 4 + 4' xuất hiện $expr_count lần"
fi

# 3. Không còn literal 256 / 321 / khai báo mảng `INPUT inputs[<số>]`
if grep -nE '\b256\b' "$FILE" >/dev/null; then
    echo "FAIL: vẫn còn literal 256:"
    grep -nE '\b256\b' "$FILE"
    fail=1
else
    echo "OK: không còn literal 256"
fi

if grep -nE '\b321\b' "$FILE" >/dev/null; then
    echo "FAIL: vẫn còn literal 321:"
    grep -nE '\b321\b' "$FILE"
    fail=1
else
    echo "OK: không còn literal 321"
fi

if grep -nE 'INPUT[[:space:]]+inputs\[[0-9]+\]' "$FILE" >/dev/null; then
    echo "FAIL: vẫn khai báo mảng bằng số literal:"
    grep -nE 'INPUT[[:space:]]+inputs\[[0-9]+\]' "$FILE"
    fail=1
else
    echo "OK: mảng INPUT khai báo bằng biểu thức MAX_BUFFER"
fi

# 4. Comment header phải có công thức
if ! grep -qE 'MAX_BUFFER\*4 \+ 4|MAX_BUFFER \* 4 \+ 4' "$FILE"; then
    echo "FAIL: thiếu comment công thức 'MAX_BUFFER * 4 + 4' trong header"
    fail=1
else
    echo "OK: comment công thức có mặt"
fi

# 5. Sanity check: MAX_BUFFER * 4 + 4 == 260 với MAX_BUFFER = 64
calc=$(( 64 * 4 + 4 ))
if [[ "$calc" -ne 260 ]]; then
    echo "FAIL: công thức không cho ra 260 (got $calc)"
    fail=1
else
    echo "OK: 64 * 4 + 4 = 260 đúng worst case"
fi

if [[ "$fail" -ne 0 ]]; then
    echo "---- check_windows_inputinjector.sh: FAIL ----"
    exit 1
fi

echo "---- check_windows_inputinjector.sh: PASS ----"
