#!/usr/bin/env bash
# test/check_size.sh — Đo kích thước binary release của Cay và so sánh với
# baseline (Phase 0) để gate refactor (Requirement 12: tăng tối đa 10%).
#
# Cách dùng:
#   bash test/check_size.sh                       # Đo + so sánh với baseline
#   bash test/check_size.sh --record-baseline     # Ghi baseline vào file mặc định
#   bash test/check_size.sh --record-baseline F   # Ghi baseline vào file F
#   bash test/check_size.sh --baseline F          # Đo + so sánh với baseline F
#
# Targets được đo (chỉ những file tồn tại):
#   - macOS  : build/cay.app/Contents/MacOS/cay (app bundle binary)
#   - macOS  : build/cay                        (plain CLI binary, nếu có)
#   - fcitx5 : build/src/platform/fcitx5/libcayime.so (Linux/Fcitx5 build)
#
# Exit codes:
#   0  : tất cả binaries trong ngưỡng ≤ 110% baseline (hoặc không có baseline)
#   1  : ít nhất một binary > 110% baseline (regression vượt mức cho phép)
#   2  : lỗi sử dụng (sai tham số, không tìm thấy binary nào, v.v.)
#
# Format output bảng:
#   Platform | Binary                                 | Size (bytes) | Size (KB)
#
# Baseline file format (text):
#   # comment lines bắt đầu '#'
#   <platform>|<relative_binary_path>|<bytes>
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEFAULT_BASELINE="$REPO_ROOT/test/baseline_size.txt"

# ---------------------------------------------------------------------------
# Targets — array song song: TARGET_PLATFORMS[i] / TARGET_PATHS[i]
# Đường dẫn relative tới REPO_ROOT.
# ---------------------------------------------------------------------------
TARGET_PLATFORMS=("macOS"                            "macOS"      "fcitx5")
TARGET_PATHS=("build/cay.app/Contents/MacOS/cay" "build/cay"  "build/src/platform/fcitx5/libcayime.so")

# ---------------------------------------------------------------------------
# get_size <abs_path> -> bytes (portable macOS + Linux). Echo "" nếu thiếu.
# ---------------------------------------------------------------------------
get_size() {
    local f="$1"
    [ -f "$f" ] || { echo ""; return; }
    case "$(uname -s)" in
        Darwin|FreeBSD|NetBSD|OpenBSD)
            stat -f %z "$f"
            ;;
        *)
            stat -c %s "$f"
            ;;
    esac
}

# ---------------------------------------------------------------------------
# print_table_header / print_table_row — bảng cố định cột.
# ---------------------------------------------------------------------------
print_table_header() {
    printf "%-8s | %-45s | %12s | %9s\n" "Platform" "Binary" "Size (bytes)" "Size (KB)"
    printf -- "---------+-----------------------------------------------+--------------+----------\n"
}

print_table_row() {
    local platform="$1"
    local rel_path="$2"
    local bytes="$3"
    local kb
    # Float formatting: KB = bytes / 1024 với 1 chữ số thập phân.
    # awk có sẵn trên cả macOS và Linux.
    kb=$(awk -v b="$bytes" 'BEGIN { printf "%.1f", b / 1024.0 }')
    printf "%-8s | %-45s | %12s | %9s\n" "$platform" "$rel_path" "$bytes" "$kb"
}

# ---------------------------------------------------------------------------
# read_baseline_size <baseline_file> <platform> <rel_path> -> bytes hoặc ""
# ---------------------------------------------------------------------------
read_baseline_size() {
    local baseline="$1"
    local platform="$2"
    local rel_path="$3"
    [ -f "$baseline" ] || { echo ""; return; }
    # Match dòng "<platform>|<rel_path>|<bytes>", bỏ qua comment.
    awk -F'|' -v p="$platform" -v rp="$rel_path" '
        /^[[:space:]]*#/ { next }
        NF == 3 && $1 == p && $2 == rp { print $3; exit }
    ' "$baseline"
}

# ---------------------------------------------------------------------------
# record_mode <out_file> — đo kích thước hiện tại, ghi vào out_file.
# ---------------------------------------------------------------------------
record_mode() {
    local out_file="$1"
    local i found=0
    local tmp
    tmp="$(mktemp -t cay_baseline.XXXXXX)"

    {
        echo "# Cay binary size baseline — recorded by test/check_size.sh"
        echo "# Format: <platform>|<relative_path>|<bytes>"
        echo "# Used by tasks.md task 8.5/8.6 (Requirement 12: tăng tối đa 10%)."
    } > "$tmp"

    for ((i = 0; i < ${#TARGET_PATHS[@]}; i++)); do
        local platform="${TARGET_PLATFORMS[$i]}"
        local rel="${TARGET_PATHS[$i]}"
        local abs="$REPO_ROOT/$rel"
        local bytes
        bytes=$(get_size "$abs")
        if [ -z "$bytes" ]; then
            echo "[record] skip (missing): $rel" >&2
            continue
        fi
        echo "$platform|$rel|$bytes" >> "$tmp"
        found=1
    done

    if [ "$found" -eq 0 ]; then
        echo "ERROR: không tìm thấy binary nào để record. Build trước (cmake --build build --target cay)." >&2
        rm -f "$tmp"
        exit 2
    fi

    mv "$tmp" "$out_file"
    echo "Wrote baseline: $out_file"
    echo
    cat "$out_file"
}

# ---------------------------------------------------------------------------
# compare_mode <baseline_file> — đo + so sánh + exit 1 nếu vượt 110%.
# ---------------------------------------------------------------------------
compare_mode() {
    local baseline="$1"
    local has_baseline=0
    [ -f "$baseline" ] && has_baseline=1

    print_table_header

    local i found=0
    local fail=0
    for ((i = 0; i < ${#TARGET_PATHS[@]}; i++)); do
        local platform="${TARGET_PLATFORMS[$i]}"
        local rel="${TARGET_PATHS[$i]}"
        local abs="$REPO_ROOT/$rel"
        local bytes
        bytes=$(get_size "$abs")
        if [ -z "$bytes" ]; then
            continue
        fi
        found=1
        print_table_row "$platform" "$rel" "$bytes"

        if [ "$has_baseline" -eq 1 ]; then
            local base
            base=$(read_baseline_size "$baseline" "$platform" "$rel")
            if [ -z "$base" ]; then
                printf "         | %-45s | %12s | %9s\n" "↑ no baseline entry — skipped" "" ""
            else
                # Ngưỡng = floor(base * 1.10). Dùng awk để tránh integer overflow.
                local threshold ratio_pct
                threshold=$(awk -v b="$base" 'BEGIN { printf "%d", b * 1.10 }')
                ratio_pct=$(awk -v c="$bytes" -v b="$base" 'BEGIN { if (b==0) print "n/a"; else printf "%.1f", (c * 100.0) / b }')
                if [ "$bytes" -gt "$threshold" ]; then
                    printf "         | %-45s | baseline=%-6s | %s%%\n" "↑ FAIL > 110% baseline" "$base" "$ratio_pct"
                    fail=1
                else
                    printf "         | %-45s | baseline=%-6s | %s%%\n" "↑ ok  ≤ 110% baseline" "$base" "$ratio_pct"
                fi
            fi
        fi
    done

    if [ "$found" -eq 0 ]; then
        echo
        echo "ERROR: không tìm thấy binary nào để đo. Build trước (cmake --build build --target cay)." >&2
        exit 2
    fi

    if [ "$has_baseline" -eq 0 ]; then
        echo
        echo "WARN: chưa có baseline tại $baseline. Chạy '$0 --record-baseline' để ghi."
        exit 0
    fi

    if [ "$fail" -ne 0 ]; then
        echo
        echo "FAIL: ít nhất một binary vượt 110% baseline (Requirement 12 — tăng tối đa 10%)."
        exit 1
    fi
    echo
    echo "PASS: tất cả binary ≤ 110% baseline."
}

# ---------------------------------------------------------------------------
# Main: parse args.
# ---------------------------------------------------------------------------
usage() {
    cat <<EOF
Usage: $0 [--record-baseline [PATH]] [--baseline PATH]

  (no args)               Đo + so sánh với $DEFAULT_BASELINE.
  --record-baseline       Ghi baseline vào $DEFAULT_BASELINE.
  --record-baseline PATH  Ghi baseline vào PATH.
  --baseline PATH         Đo + so sánh với PATH.
EOF
}

main() {
    local mode="compare"
    local out_file="$DEFAULT_BASELINE"
    local baseline_file="$DEFAULT_BASELINE"

    while [ $# -gt 0 ]; do
        case "$1" in
            --record-baseline)
                mode="record"
                shift
                if [ $# -gt 0 ] && [ "${1:0:2}" != "--" ]; then
                    out_file="$1"
                    shift
                fi
                ;;
            --baseline)
                shift
                if [ $# -eq 0 ]; then
                    echo "ERROR: --baseline cần PATH" >&2
                    usage >&2
                    exit 2
                fi
                baseline_file="$1"
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                echo "ERROR: unknown arg '$1'" >&2
                usage >&2
                exit 2
                ;;
        esac
    done

    if [ "$mode" = "record" ]; then
        record_mode "$out_file"
    else
        compare_mode "$baseline_file"
    fi
}

main "$@"
