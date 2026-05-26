#!/usr/bin/env bash
# check_refactor.sh — Verify refactor không phá vỡ baseline
# Chạy từ root: bash test/check_refactor.sh
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD=build_baseline

echo "==> Build cay_test"
cmake --build $BUILD --target cay_test -j8 >/dev/null

echo "==> Run cay_test (full doctest suite)"
# Sau regen baseline_traces.csv: tất cả test PASS — exit 0.
FULL=$(./$BUILD/cay_test 2>&1)
RESULT=$(echo "$FULL" | grep -E "test cases:|assertions:")
echo "$RESULT"

# Sau regenerate test/baseline_traces.csv → tất cả test phải PASS.
EXPECT_CASES="21"
EXPECT_PASSED="21"
EXPECT_FAILED="0"

CASES=$(echo "$RESULT" | grep -oE 'test cases: *[0-9]+' | grep -oE '[0-9]+' | head -1)
PASSED=$(echo "$RESULT" | grep -oE '[0-9]+ passed' | head -1 | grep -oE '[0-9]+')
FAILED=$(echo "$RESULT" | grep -oE '[0-9]+ failed' | head -1 | grep -oE '[0-9]+')

echo "==> Cases=$CASES Passed=$PASSED Failed=$FAILED (baseline: $EXPECT_CASES/$EXPECT_PASSED/$EXPECT_FAILED)"

if [[ "$CASES" != "$EXPECT_CASES" || "$PASSED" != "$EXPECT_PASSED" || "$FAILED" != "$EXPECT_FAILED" ]]; then
    echo "FAIL: Test counts thay đổi so với baseline."
    exit 1
fi

echo "OK: Refactor giữ đúng baseline test."
