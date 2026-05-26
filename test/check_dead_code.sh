#!/usr/bin/env bash
# ============================================================================
# check_dead_code.sh — Audit public API của CayData/CayEngine sau Phase 7.
#
# Cho mỗi method public khai báo trong `src/core/CayData.h` và
# `src/core/CayEngine.h`, đếm số call site trong `src/` (production) và
# `test/` (tests). Flag method nếu:
#
#   (a) không có caller trong `src/` (loại trừ dòng định nghĩa của method),
#       VÀ
#   (b) trên 6 dòng comment ngay trước declaration KHÔNG có một trong các
#       từ khoá giải thích: "test-only", "platform contract",
#       "Public utility", "currently no-op", "tests only".
#
# Cách đếm:
#   - Match `\b<method>(` tại bất kỳ đâu trong src/ và test/.
#   - Loại trừ dòng định nghĩa: `<ret_type> <class>::<method>(`.
#   - Loại trừ dòng khai báo trong header (.h): `static <ret> <method>(` hoặc
#     `<ret> <method>(` lùi đầu dòng.
#
# Script không sửa file — chỉ in báo cáo và exit ≠ 0 nếu có flag.
#
# Sử dụng:
#   bash test/check_dead_code.sh
# ============================================================================
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HEADER_DATA="$ROOT/src/core/CayData.h"
HEADER_ENGINE="$ROOT/src/core/CayEngine.h"

if [ ! -f "$HEADER_DATA" ] || [ ! -f "$HEADER_ENGINE" ]; then
  echo "FAIL: missing CayData.h or CayEngine.h" >&2
  exit 1
fi

flagged=0
total=0

count_calls() {
  local class="$1"
  local method="$2"
  local dir="$3"

  local raw
  raw=$(grep -rnE --include='*.cpp' --include='*.mm' --include='*.h' \
        "\\b${method}\\(" "$dir" 2>/dev/null)

  if [ -z "$raw" ]; then
    echo 0
    return
  fi

  # Loại trừ dòng định nghĩa method (`<ret_type> <class>::<method>(`).
  raw=$(echo "$raw" | grep -vE "[a-zA-Z_][a-zA-Z_0-9 <>*&]* ${class}::${method}\\(")

  # Loại trừ khai báo trong header (.h) — bắt đầu bằng whitespace + return type.
  raw=$(echo "$raw" | grep -vE "\\.h:[0-9]+:[[:space:]]+(static[[:space:]]+)?[a-zA-Z_][a-zA-Z_0-9 <>*&]*[ &*]${method}[[:space:]]*\\(")

  # Loại trừ comment-only references trên header (lines bắt đầu bằng `//`).
  raw=$(echo "$raw" | grep -vE "\\.h:[0-9]+://")

  if [ -z "$raw" ]; then
    echo 0
  else
    echo "$raw" | wc -l | tr -d ' '
  fi
}

audit_method() {
  local class="$1"
  local method="$2"
  local header="$3"

  total=$((total + 1))

  local src_calls
  src_calls=$(count_calls "$class" "$method" "$ROOT/src")
  local test_calls
  test_calls=$(count_calls "$class" "$method" "$ROOT/test")

  local decl_line
  decl_line=$(grep -nE "^[[:space:]]+(static[[:space:]]+)?[a-zA-Z_][a-zA-Z_0-9 <>*&]*[ &*]${method}[[:space:]]*\\(" "$header" \
              | head -1 | cut -d: -f1)

  local has_excuse=0
  if [ -n "$decl_line" ]; then
    local start=$((decl_line - 6))
    [ "$start" -lt 1 ] && start=1
    if sed -n "${start},${decl_line}p" "$header" \
       | grep -qiE "test[ -]only|platform contract|public utility|currently no-op|tests only"; then
      has_excuse=1
    fi
  fi

  local status="ok"
  if [ "$src_calls" -eq 0 ] && [ "$test_calls" -eq 0 ] && [ "$has_excuse" -eq 0 ]; then
    status="DEAD (no callers, no excuse)"
    flagged=$((flagged + 1))
  elif [ "$src_calls" -eq 0 ] && [ "$has_excuse" -eq 0 ]; then
    status="TEST-ONLY without comment"
    flagged=$((flagged + 1))
  fi

  printf '  %-32s src=%-3s test=%-3s comment=%s  %s\n' \
    "${class}::${method}" "$src_calls" "$test_calls" "$has_excuse" "$status"
}

echo "=== Audit CayData public methods ==="
for m in IsValidInitial IsValidNucleus GetToneIndex GetToneMark \
         HasVietnameseMark StripTone StripAccent IsVowel GetHookRule \
         ToLowerViet ToUpperViet DecomposeChar ComposeChar \
         TryMatchInitial TryMatchNucleus TryMatchFinal TryMatchTail; do
  audit_method "CayData" "$m" "$HEADER_DATA"
done

echo
echo "=== Audit TelexEngine public methods ==="
for m in OnKeyDown OnKeyUp ResetFull CommitWord; do
  audit_method "TelexEngine" "$m" "$HEADER_ENGINE"
done

echo
echo "Total methods: $total"
echo "Flagged:       $flagged"

if [ "$flagged" -gt 0 ]; then
  echo "FAIL: dead code or undocumented test-only API detected" >&2
  exit 1
fi

echo "PASS: every public method has a caller or an explanatory comment"
exit 0
