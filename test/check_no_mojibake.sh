#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# check_no_mojibake.sh
#
# Phát hiện mojibake (chữ vỡ Unicode) trong các file C++ ở src/core/.
# Mojibake thường xuất hiện ở 2 dạng:
#   1. Ký tự U+FFFD (REPLACEMENT CHARACTER) — chuỗi byte 0xEF 0xBF 0xBD
#      → kết quả khi UTF-8 decoder gặp byte không hợp lệ.
#   2. Dấu '?' rải rác trong comment tiếng Việt — kết quả khi file gốc encode
#      Windows-1258/CP1252 bị save lại dưới dạng UTF-8 với "?" cho byte không
#      decode được.
#
# Verify cho task 7.2 — Requirement 15.2, 15.3.
# Exit code 0 nếu sạch, khác 0 nếu phát hiện mojibake.
# ---------------------------------------------------------------------------

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGETS=("$ROOT_DIR/src/core/CayEngine.cpp")

EXIT_CODE=0

echo "==> Quét U+FFFD (REPLACEMENT CHARACTER)..."
for f in "${TARGETS[@]}"; do
    if grep -n $'\xef\xbf\xbd' "$f" > /dev/null 2>&1; then
        echo "FAIL: $f còn ký tự U+FFFD ở các dòng:"
        grep -n $'\xef\xbf\xbd' "$f"
        EXIT_CODE=1
    else
        echo "OK:   $f không có U+FFFD"
    fi
done

echo ""
echo "==> Quét dấu '?' trong comment (loại trừ ternary)..."
# Heuristic: tìm dòng có '?' trong comment (// ... ? ...) mà không phải ternary.
# Pattern ternary chứa ' ? ' với toán hạng 2 bên.
# Pattern mojibake: '?' liền các chữ Việt không dấu (u, i, e, ...) hoặc bên cạnh space + lowercase.
for f in "${TARGETS[@]}"; do
    # Lọc dòng chứa "//" và có '?', loại bỏ các dòng có pattern ternary hoặc câu hỏi tiếng Anh.
    suspicious=$(awk '
        /\/\// {
            comment_part = substr($0, index($0, "//"))
            # Bỏ qua nếu comment kết thúc bằng "?" (câu hỏi tiếng Anh hợp lệ)
            # và không có "?" nào khác trong comment
            n = gsub(/\?/, "&", comment_part)
            if (n == 0) next
            # Có >= 2 dấu '?' trong comment → khả năng cao là mojibake
            if (n >= 2) {
                printf("%d:%s\n", NR, $0)
                next
            }
            # Có đúng 1 dấu '?' và không phải ở cuối câu → có thể mojibake
            sub(/[ \t]+$/, "", comment_part)
            if (comment_part !~ /\?$/) {
                printf("%d:%s\n", NR, $0)
            }
        }
    ' "$f" || true)

    if [ -n "$suspicious" ]; then
        echo "WARN: $f có thể còn mojibake trong comment:"
        echo "$suspicious"
        EXIT_CODE=1
    else
        echo "OK:   $f không có '?' đáng ngờ trong comment"
    fi
done

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo "==> Tất cả check pass."
else
    echo "==> Phát hiện mojibake. Vui lòng sửa lại."
fi

exit $EXIT_CODE
