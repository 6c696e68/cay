#!/usr/bin/env bash
# Verify task 7.3: terminal program names refactored into g_terminalProgramNames array.
set -euo pipefail

FILE="src/platform/fcitx5/CayimeEngine.cpp"

if [[ ! -f "$FILE" ]]; then
    echo "FAIL: $FILE not found"
    exit 1
fi

fail=0

# 1. Array declaration must exist with std::string_view.
if ! grep -q 'static const std::string_view g_terminalProgramNames\[\]' "$FILE"; then
    echo "FAIL: g_terminalProgramNames[] declaration not found"
    fail=1
fi

# 2. <string_view> must be included.
if ! grep -q '#include <string_view>' "$FILE"; then
    echo "FAIL: <string_view> include missing"
    fail=1
fi

# 3. Comment about VTE bug must exist.
if ! grep -q 'VTE-based terminals' "$FILE"; then
    echo "FAIL: VTE comment missing"
    fail=1
fi
if ! grep -q 'ignore deleteSurroundingText' "$FILE"; then
    echo "FAIL: comment about deleteSurroundingText missing"
    fail=1
fi

# 4. All 7 names must be present in array.
for name in terminal alacritty kitty konsole terminator wezterm tmux; do
    # match the literal "name" within array region
    if ! grep -q "\"$name\"" "$FILE"; then
        echo "FAIL: terminal name \"$name\" missing"
        fail=1
    fi
done

# 5. Disjunction chain must be gone.
if grep -q 'prog\.find("terminal").*||.*prog\.find("alacritty")' "$FILE"; then
    echo "FAIL: old disjunction chain still present"
    fail=1
fi

# Count occurrences of `prog.find(` — should appear exactly once now (inside the loop).
count=$(grep -c 'prog\.find(' "$FILE" || true)
if [[ "$count" -ne 1 ]]; then
    echo "FAIL: expected 1 prog.find( call, found $count"
    fail=1
fi

# 6. for-loop iterating the array must exist.
if ! grep -q 'for (const auto& name : g_terminalProgramNames)' "$FILE"; then
    echo "FAIL: for-loop over g_terminalProgramNames not found"
    fail=1
fi

if [[ "$fail" -eq 0 ]]; then
    echo "OK: task 7.3 verification passed"
    exit 0
else
    exit 1
fi
