#!/usr/bin/env bash
# test/check_invariants.sh — Smoke check 5 invariant cho refactor src/core/.
#
# 5 check (theo Requirements 2.1, 4.7, 5.1, 11.1, 15.1, 15.2, 15.3):
#   (a) check_single_source_of_truth — tables s_initials/s_nuclei/s_finals/s_tails
#       chỉ tồn tại trong 1 file (CayData.cpp).
#   (b) check_no_repeat_for_loop — số "for (int t = 1; t <= 5;" trong
#       src/core/CayEngine.cpp ≤ 1.
#   (c) check_max_buffer_constexpr — MAX_BUFFER không còn #define, phải là
#       `constexpr int MAX_BUFFER` trong CayTypes.h.
#   (d) check_no_stl_in_core — không include STL trong src/core/.
#   (e) check_utf8_no_bom — src/core/*.cpp/.h là UTF-8 không BOM.
#
# Lưu ý: Phase 0 chạy script này có thể FAIL ở (a)(c)(d)(e) — expected,
# vì các phase sau (P3, P5, P6) mới fix. Script là gate sau Phase 6.
# Phase 0 chỉ cần: file tồn tại + `bash -n` xanh.
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CORE_DIR="$REPO_ROOT/src/core"

# ---------------------------------------------------------------------------
# (a) Single source of truth
# ---------------------------------------------------------------------------
check_single_source_of_truth() {
    local name="check_single_source_of_truth"
    local hits
    hits=$(grep -lE "static const wchar_t\* const s_(initials|nuclei|finals|tails)" \
                 -r "$CORE_DIR" 2>/dev/null || true)
    local count=0
    if [ -n "$hits" ]; then
        count=$(printf '%s\n' "$hits" | grep -c '.' || true)
    fi
    if [ "$count" -ne 1 ]; then
        echo "[$name] FAIL: tables s_initials/s_nuclei/s_finals/s_tails xuất hiện trong $count file (kỳ vọng 1, chỉ ở CayData.cpp):"
        if [ -n "$hits" ]; then
            printf '  %s\n' $hits
        fi
        exit 1
    fi
    echo "[$name] PASS"
}

# ---------------------------------------------------------------------------
# (b) No repeated for loop
# ---------------------------------------------------------------------------
check_no_repeat_for_loop() {
    local name="check_no_repeat_for_loop"
    local file="$CORE_DIR/CayEngine.cpp"
    if [ ! -f "$file" ]; then
        echo "[$name] FAIL: không tìm thấy $file"
        exit 1
    fi
    local count
    count=$(grep -cE "for \(int t = 1; t <= 5;" "$file" || true)
    if [ "$count" -gt 1 ]; then
        echo "[$name] FAIL: phát hiện $count lần 'for (int t = 1; t <= 5;' trong CayEngine.cpp (kỳ vọng ≤ 1)"
        grep -nE "for \(int t = 1; t <= 5;" "$file" | sed 's/^/  /'
        exit 1
    fi
    echo "[$name] PASS"
}

# ---------------------------------------------------------------------------
# (c) MAX_BUFFER constexpr
# ---------------------------------------------------------------------------
check_max_buffer_constexpr() {
    local name="check_max_buffer_constexpr"
    local define_hits
    define_hits=$(grep -rnE "^#define MAX_BUFFER" "$CORE_DIR" 2>/dev/null || true)
    if [ -n "$define_hits" ]; then
        echo "[$name] FAIL: vẫn còn '#define MAX_BUFFER' trong src/core/ (kỳ vọng 0):"
        printf '  %s\n' "$define_hits"
        exit 1
    fi
    local types_h="$CORE_DIR/CayTypes.h"
    if [ ! -f "$types_h" ]; then
        echo "[$name] FAIL: không tìm thấy $types_h"
        exit 1
    fi
    local constexpr_count
    constexpr_count=$(grep -cE "constexpr int MAX_BUFFER" "$types_h" || true)
    if [ "$constexpr_count" -lt 1 ]; then
        echo "[$name] FAIL: không tìm thấy 'constexpr int MAX_BUFFER' trong $types_h (kỳ vọng ≥ 1)"
        exit 1
    fi
    echo "[$name] PASS"
}

# ---------------------------------------------------------------------------
# (d) No STL in src/core/
# ---------------------------------------------------------------------------
check_no_stl_in_core() {
    local name="check_no_stl_in_core"
    local stl_pattern='^#include <(vector|string|map|unordered_map|algorithm|memory|iostream|sstream|fstream|array|deque|list|set|unordered_set|stack|queue|tuple|optional|variant|functional|chrono|thread|mutex|atomic|future|exception|stdexcept|new|typeinfo|cassert|cmath|cstdlib|cstring|cstdio)>'
    local hits
    hits=$(grep -rnE "$stl_pattern" "$CORE_DIR" 2>/dev/null || true)
    if [ -n "$hits" ]; then
        echo "[$name] FAIL: phát hiện include STL trong src/core/ (vi phạm no-CRT/no-STL):"
        printf '  %s\n' "$hits"
        exit 1
    fi
    echo "[$name] PASS"
}

# ---------------------------------------------------------------------------
# (e) UTF-8 không BOM
# ---------------------------------------------------------------------------
check_utf8_no_bom() {
    local name="check_utf8_no_bom"
    local bad=()
    local f
    # BOM UTF-8: byte 0xEF 0xBB 0xBF (octal 357 273 277)
    for f in "$CORE_DIR"/*.cpp "$CORE_DIR"/*.h; do
        [ -f "$f" ] || continue
        if head -c 3 "$f" | od -An -c | grep -q "357 273 277"; then
            bad+=("$f")
        fi
    done
    if [ "${#bad[@]}" -gt 0 ]; then
        echo "[$name] FAIL: phát hiện BOM UTF-8 ở các file (kỳ vọng UTF-8 không BOM):"
        printf '  %s\n' "${bad[@]}"
        exit 1
    fi
    echo "[$name] PASS"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
main() {
    echo "== check_invariants.sh =="
    echo "REPO_ROOT: $REPO_ROOT"
    echo

    check_single_source_of_truth
    check_no_repeat_for_loop
    check_max_buffer_constexpr
    check_no_stl_in_core
    check_utf8_no_bom

    echo
    echo "== All 5 invariant checks PASSED =="
}

main "$@"
