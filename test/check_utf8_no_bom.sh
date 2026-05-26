#!/usr/bin/env bash
# test/check_utf8_no_bom.sh
# Verify src/core/*.cpp và src/core/*.h là UTF-8 KHÔNG BOM.
# - In rõ từng file đã check.
# - Exit 0 nếu không file nào có BOM.
# - Exit 1 nếu có ít nhất 1 file có BOM (3 byte EF BB BF ở đầu file).
#
# Idempotent: chạy nhiều lần không gây tác dụng phụ.

set -u

# Cố định working dir = root repo (script nằm trong test/, root là parent).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CORE_DIR="${REPO_ROOT}/src/core"

if [[ ! -d "${CORE_DIR}" ]]; then
    echo "ERROR: Không tìm thấy ${CORE_DIR}" >&2
    exit 2
fi

# Liệt kê file: cố định danh sách 5 file theo tasks.md để tránh phụ thuộc shell glob.
FILES=(
    "${CORE_DIR}/CayData.cpp"
    "${CORE_DIR}/CayData.h"
    "${CORE_DIR}/CayEngine.cpp"
    "${CORE_DIR}/CayEngine.h"
    "${CORE_DIR}/CayTypes.h"
)

bom_count=0
checked=0
missing=0

printf "%-12s | %-40s | %s\n" "Status" "File" "First 3 bytes (hex)"
printf -- "-------------+------------------------------------------+--------------------\n"

for f in "${FILES[@]}"; do
    if [[ ! -f "${f}" ]]; then
        printf "%-12s | %-40s | %s\n" "MISSING" "${f#${REPO_ROOT}/}" "(file not found)"
        missing=$((missing + 1))
        continue
    fi

    checked=$((checked + 1))

    # Đọc 3 byte đầu, chuyển sang hex (lowercase, không space).
    head_hex="$(head -c 3 "${f}" | od -An -tx1 -v | tr -d ' \n')"

    if [[ "${head_hex}" == "efbbbf" ]]; then
        printf "%-12s | %-40s | %s\n" "BOM" "${f#${REPO_ROOT}/}" "${head_hex}"
        bom_count=$((bom_count + 1))
    else
        printf "%-12s | %-40s | %s\n" "OK (no BOM)" "${f#${REPO_ROOT}/}" "${head_hex}"
    fi
done

echo ""
echo "Summary: checked=${checked}, with_bom=${bom_count}, missing=${missing}"

if [[ ${missing} -gt 0 ]]; then
    echo "FAIL: có ${missing} file không tồn tại — kiểm tra lại danh sách."
    exit 2
fi

if [[ ${bom_count} -gt 0 ]]; then
    echo "FAIL: ${bom_count} file có BOM. Cần strip BOM bằng test/strip_bom.py hoặc editor."
    exit 1
fi

echo "PASS: tất cả ${checked} file là UTF-8 không BOM."
exit 0
