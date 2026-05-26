# Implementation Plan — Engine Core Refactor

## Overview

Kế hoạch triển khai chia refactor `src/core/` của Cay thành 7 phase tuần tự. Mỗi phase commit độc lập, build pass trên cả 3 platform (Windows no-CRT, macOS bundle, Fcitx5), và test PBT + golden trace xanh. Tổng cộng 17 property tests bao phủ Requirements 1–18, plus smoke checks bao phủ Requirements 2, 5, 11, 14, 15.

Ngôn ngữ triển khai: **C++17** (đã có sẵn trong project). Test target tách riêng (`BUILD_TESTING=ON`) được phép dùng STL — release binary giữ nguyên ràng buộc no-CRT / no-STL / no-allocation / no-exception.

Quy ước file:
- File test C++ → `test/test_<scope>.cpp` (chạy bởi binary `cay_test`).
- File check shell → `test/check_<scope>.sh` (chạy độc lập).
- Header test-only → `test/property.h`, `test/corpus.h`.
- Snapshot → `test/baseline_traces.csv`.
- KHÔNG tạo file `.md` ngoài `.kiro/specs/engine-core-refactor/`.

Sau mỗi task hoàn thành, lệnh sau phải xanh:
```
cmake -B build -DBUILD_TESTING=ON && cmake --build build && ctest --test-dir build --output-on-failure
bash test/check_invariants.sh
```

## Tasks

- [x] 1. Phase 0 — Setup test infrastructure và golden baseline
  - [x] 1.1 Thêm `BUILD_TESTING` option và target `cay_test` vào `CMakeLists.txt`
    - Thêm `option(BUILD_TESTING "Build property-based tests" OFF)` vào `CMakeLists.txt` ở scope top-level (sau khối `BUILD_FCITX5`).
    - Khi `BUILD_TESTING=ON`: `enable_testing()`, tạo `add_executable(cay_test ${CORE_SOURCES} test/...)`, set `target_compile_definitions(cay_test PRIVATE CAY_TEST_BUILD)`, `target_include_directories(cay_test PRIVATE src/core test)`, `add_test(NAME cay_test COMMAND cay_test)`.
    - KHÔNG áp dụng `/EHs-c-`, `/GR-`, `/NODEFAULTLIB:msvcrt.lib` cho `cay_test` (test build dùng STL bình thường).
    - Verify: `cmake -B build -DBUILD_TESTING=ON` cấu hình thành công trên macOS.
    - _Requirements: 1.4, 11.5_

  - [x] 1.2 Thêm doctest single-header và `test/property.h` mini PBT layer
    - Tải `doctest.h` (single-header v2.4.x, MIT) vào `test/doctest.h`.
    - Tạo `test/property.h` với: struct `Gen<T>`, generator `genVowelChar()` (12 base × 6 tone × 2 case = 144 ký tự), `genTelexSyllable()` (initial + nucleus + final + tail hợp lệ), `genKeySeq(int maxLen)`, runner `forAll(gen, iterations, pred)`.
    - Tạo `test/property.h` chứa `MockInjector` struct với `static void Hook(int bs, const wchar_t*, int)` để mock `OnInjectText`.
    - Cố định seed mặc định `42`, cho phép override qua env var `CAY_TEST_SEED`.
    - _Requirements: 1.4_

  - [x] 1.3 Tạo `test/corpus.h` với 200 từ Việt + 50 từ Anh annotated
    - File header-only `test/corpus.h` chứa 2 array `static const wchar_t* const g_vietnameseCorpus[200]` và `static const char* const g_englishCorpus[50]`.
    - Mỗi entry tiếng Việt là cặp `{telex_input, expected_output}` (ví dụ `{L"toi", L"tôi"}`, `{L"truowngf", L"trường"}`).
    - 200 từ Việt: 100 từ phổ thông tần suất cao + 50 từ tổ hợp dấu khó (mờ/mở/mỡ, hươu, nguyễn, giường, quýt, ...) + 50 từ test FindTonePosition (1/2/3 nguyên âm, có/không phụ âm cuối).
    - 50 từ Anh: bao gồm `class`, `style`, `block`, `function`, `public`, `width`, `flag`, `zoom`, `quick`, ... bao trùm các bypass rules (Requirement 8).
    - _Requirements: 1.2, 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 8.1, 8.2, 8.3, 8.4, 8.5_

  - [x] 1.4 Tạo `test/check_invariants.sh` smoke check script
    - Shell script bash kiểm tra 5 invariant: (a) tables `s_initials/s_nuclei/s_finals/s_tails` chỉ tồn tại trong 1 file, (b) số `for (int t = 1; t <= 5;)` trong `src/core/CayEngine.cpp` ≤ 1, (c) `MAX_BUFFER` không còn `#define`, (d) không include STL trong `src/core/`, (e) `src/core/*.cpp` và `src/core/*.h` là UTF-8 không BOM.
    - Mỗi check fail-fast với exit code khác 0 và in rõ check nào fail.
    - Đặt file thực thi: `chmod +x test/check_invariants.sh`.
    - Lưu ý: Phase 0 chạy script này có thể fail ở (a)(c)(d)(e) — đó là expected vì các phase sau mới fix. Script viết để dùng làm gate sau Phase 6. Phase 0 chỉ cần script tồn tại và parse được (`bash -n test/check_invariants.sh`).
    - _Requirements: 2.1, 4.7, 5.1, 11.1, 15.1, 15.2, 15.3_

  - [x] 1.5 Tạo `test/golden_trace.cpp` cơ chế record + check golden trace
    - Test target `cay_test` nhận arg `--record-golden` để generate trace cho 1000 random key sequences (seed 42, độ dài 1..32) và ghi vào `test/baseline_traces.csv`.
    - Format CSV: `seed,key_sequence_hex,trace_hex` với `trace_hex` là concat của tất cả `(backspaceCount, newText, newTextLen)` từ `MockInjector`.
    - Test target chạy không arg sẽ load `test/baseline_traces.csv` và verify diff = 0 với trace hiện tại.
    - File `test/golden_trace.cpp` có hàm `main` (override doctest main) hoặc dùng `TEST_CASE("P15: golden trace ...")` với fixture load CSV.
    - _Requirements: 1.1, 1.2, 1.3, 7.8, 8.7, 18.3_

  - [x] 1.6 Record golden baseline tại HEAD trước refactor (P15)
    - Build `cay_test` từ source HEAD hiện tại (chưa refactor).
    - Chạy `./build/cay_test --record-golden` → sinh `test/baseline_traces.csv`.
    - Commit file CSV vào repo (kích thước dự kiến ~50KB cho 1000 sequences).
    - Verify: `./build/cay_test` (không arg) chạy xanh — trace hiện tại bằng baseline.
    - _Requirements: 1.1, 1.2, 1.3_

  - [x] 1.7 Tạo `test/test_decompose_compose.cpp` skeleton (placeholder cho P1-P4)
    - Tạo file với 4 `TEST_CASE` rỗng, comment tag `// [Feature: engine-core-refactor, Property N: <text>]`.
    - Property 1: Round-trip Decompose/Compose.
    - Property 2: Round-trip StripTone/GetToneMark.
    - Property 3: Completeness của GetToneMark.
    - Property 4: GetToneIndex inverse mapping.
    - Tests SKIP (qua `DOCTEST_SKIP`) cho đến khi Phase 1 implement helper.
    - _Requirements: 4.1, 4.2, 4.3, 9.1, 9.3, 9.4, 9.5_

  - [x] 1.8 Smoke check chạy thành công và golden trace pass
    - `bash test/check_invariants.sh` — script tồn tại, parse được (`bash -n` xanh). Một số check fail là expected ở Phase 0.
    - `cmake --build build --target cay_test && ctest --test-dir build --output-on-failure` — golden trace P15 pass với baseline vừa record.
    - Verify build release vẫn pass: `cmake --build build --target cay`.
    - _Requirements: 1.4, 11.5_

- [x] 2. Phase 1 — Thêm helper API mới (non-breaking)
  - [x] 2.1 Thêm `DecomposedChar` struct và `ToLowerViet`/`ToUpperViet` vào `CayData.h`
    - Khai báo `struct DecomposedChar { wchar_t base; int toneIndex; bool isUpper; }` trong namespace `Cay`, scope public của `CayData` (hoặc namespace-level cạnh `CayData`).
    - Khai báo `static wchar_t CayData::ToLowerViet(wchar_t c)`, `static wchar_t CayData::ToUpperViet(wchar_t c)` — di chuyển khai báo từ private static helpers trong `CayEngine.cpp`.
    - Giữ helpers cũ trong `CayEngine.cpp` wrap về `CayData::ToLowerViet/ToUpperViet` để không break (Phase 2 sẽ xoá).
    - _Requirements: 4.1, 4.2_

  - [x] 2.2 Implement `CayData::ToLowerViet`, `ToUpperViet`, `DecomposeChar`, `ComposeChar` trong `CayData.cpp`
    - `ToLowerViet`/`ToUpperViet`: copy từ `CayEngine.cpp` (giữ nguyên logic Latin-1, U+01A0..U+01B0, U+1EA0..U+1EF8).
    - `DecomposeChar(c)`: O(1) — `isUpper = (c != ToLowerViet(c))`, `lo = ToLowerViet(c)`, `toneIndex = LookupToneIndex(lo)`, `base = StripTone(lo)`.
    - `ComposeChar(base, toneIndex, isUpper)`: O(1) — nếu `toneIndex == 0` trả `isUpper ? ToUpperViet(base) : base`; ngược lại trả `isUpper ? ToUpperViet(GetToneMark(base, toneIndex)) : GetToneMark(base, toneIndex)`.
    - Static helper `LookupToneIndex(wchar_t c_lower)`: switch ~60 case bao trùm 12 nguyên âm × 5 tone, mặc định trả 0.
    - Lưu ý: KHÔNG include header STL. Chỉ dùng `wchar_t`, `int`, `bool`.
    - _Requirements: 4.1, 4.2, 4.3, 11.1, 11.6_

  - [x] 2.3 Thêm bảng `s_finals`, `s_tails` và API `TryMatchInitial/Nucleus/Final/Tail` trong `CayData.h/.cpp`
    - Khai báo 4 static method trong `CayData`: `static int TryMatchInitial(const wchar_t* s, int len)` (longest-prefix-match), tương tự cho `TryMatchNucleus`, `TryMatchFinal`, `TryMatchTail`. Trả về số `wchar_t` đã match (0 nếu không match).
    - Implement trong `CayData.cpp`: thêm 2 array `static const wchar_t* const s_finals[]` (`L"ng"`, `L"nh"`, `L"ch"`, `L"c"`, `L"m"`, `L"n"`, `L"p"`, `L"t"`) và `static const wchar_t* const s_tails[]` (`L"i"`, `L"y"`, `L"o"`, `L"u"`).
    - `TryMatch*` duyệt array (đã sắp xếp dài-trước), match longest-prefix bằng `CayStrCmp` slice.
    - _Requirements: 2.1, 2.3, 17.1_

  - [x] 2.4 Implement Property 1 — Round-trip Decompose/Compose ký tự (`test/test_decompose_compose.cpp`)
    - **Property 1: Round-trip Decompose/Compose ký tự**
    - **Validates: Requirements 4.1, 4.2, 4.3**
    - For each ký tự trong `genVowelChar()` (144 ký tự bao gồm 12 base × 6 tone × 2 case + d/đ/D/Đ + ư/Ư/ơ/Ơ/ă/Ă/â/Â/ê/Ê/ô/Ô): assert `ComposeChar(d.base, d.toneIndex, d.isUpper) == c` với `d = DecomposeChar(c)`.
    - Bỏ DOCTEST_SKIP, test phải pass.

  - [x] 2.5 Implement Property 2 — Round-trip StripTone/GetToneMark (`test/test_decompose_compose.cpp`)
    - **Property 2: Round-trip StripTone/GetToneMark**
    - **Validates: Requirements 9.1, 9.5**
    - For each `b` ∈ {a, â, ă, e, ê, i, o, ô, ơ, u, ư, y} (lower + upper) và `toneIndex` 1..5: nếu `c = GetToneMark(b, toneIndex) != 0` thì assert `StripTone(c) == b`.

  - [x] 2.6 Implement Property 3 — Completeness của GetToneMark (`test/test_decompose_compose.cpp`)
    - **Property 3: Completeness của GetToneMark**
    - **Validates: Requirements 9.3**
    - For each base `b` ∈ {a, â, ă, e, ê, i, o, ô, ơ, u, ư, y} (lower) và `toneIndex` ∈ {1, 2, 3, 4, 5}: assert `GetToneMark(b, toneIndex) != 0`.

  - [x] 2.7 Implement Property 4 — GetToneIndex inverse mapping (`test/test_decompose_compose.cpp`)
    - **Property 4: GetToneIndex inverse mapping cho phím Telex**
    - **Validates: Requirements 9.4**
    - Verify: `GetToneIndex(L'z') == 0`, `GetToneIndex(L'f') == 1`, `GetToneIndex(L's') == 2`, `GetToneIndex(L'r') == 3`, `GetToneIndex(L'x') == 4`, `GetToneIndex(L'j') == 5`. Cùng cho uppercase. Phím khác trả `-1`.

  - [x] 2.8 Verify Phase 1 — build + golden trace + smoke
    - `cmake --build build` (release): pass trên macOS.
    - `cmake --build build --target cay_test && ctest --test-dir build --output-on-failure`: P1-P4 + golden trace P15 đều xanh.
    - Verify: code lặp `for (int t = 1; t <= 5;)` trong `CayEngine.cpp` chưa giảm (phase này chỉ thêm helper, chưa rewrite caller).
    - _Requirements: 1.1, 1.2, 1.3, 4.1, 4.2_

- [x] 3. Phase 2 — Rewrite Apply* methods dùng helper
  - [x] 3.1 Rewrite `TelexEngine::ApplyDoubleKeys` dùng `DecomposeChar`/`ComposeChar`
    - Trong `src/core/CayEngine.cpp`, thay block `for (int t = 1; t <= 5; t++) { ... }` extract tone bằng 1 dòng `auto d = CayData::DecomposeChar(target);`.
    - Dùng `d.base`, `d.toneIndex`, `d.isUpper` thay cho `baseTarget`, `tone`, `isUpper`.
    - Apply/undo logic: dùng `CayData::ComposeChar(newBase, d.toneIndex, d.isUpper)` cho tất cả assignment.
    - Giữ nguyên cấu trúc backward-scan (`for j = _textLen-1; j >= 0; j--`) và logic Telex Tell-Don't-Ask.
    - _Requirements: 4.4, 4.7, 13.4_

  - [x] 3.2 Rewrite `TelexEngine::ApplyHookKeys` dùng helper
    - Thay tất cả block `for (int t = 1; t <= 5;)` (5 chỗ trong `ApplyHookKeys`) bằng 1 dòng `auto d = CayData::DecomposeChar(target)` (tại scan chính) và `auto p = CayData::DecomposeChar(_text[j-1])` (cho previous char).
    - Logic undo (loBase ∈ {ă, ơ, ư}), logic apply (oo→ô, ow→ơ, uw→ư, aw→ă, uo→ươ, ua→ưa, uu→ưu) giữ nguyên — chỉ thay decompose.
    - _Requirements: 4.5, 4.7, 13.4_

  - [x] 3.3 Rewrite `TelexEngine::ApplyToneMarks` dùng helper
    - Thay block extract tone trong `ApplyToneMarks` bằng `auto d = CayData::DecomposeChar(_text[pos])`.
    - Logic strip-then-apply giữ nguyên: nếu `d.toneIndex == toneIndex` (gõ lại cùng tone) → strip + append raw key; ngược lại → apply tone tại `pos = FindTonePosition()`.
    - _Requirements: 4.6, 4.7, 6.1, 6.4_

  - [x] 3.4 Xoá static helper `ToLowerViet`, `ToUpperViet` cũ trong `CayEngine.cpp`
    - Xoá khai báo `static wchar_t TelexEngine::ToLowerViet`, `ToUpperViet` (trong `CayEngine.h` chuyển từ private sang xoá hoàn toàn).
    - Thay tất cả call site `ToLowerViet(...)` và `ToUpperViet(...)` trong `CayEngine.cpp` bằng `CayData::ToLowerViet(...)` / `CayData::ToUpperViet(...)`.
    - Verify: `grep -c "TelexEngine::ToLowerViet\|TelexEngine::ToUpperViet" src/core/` = 0.
    - _Requirements: 4.1, 4.2_

  - [x] 3.5 Implement Property 5 — Round-trip tone key double-press (`test/test_modifier_round_trip.cpp`)
    - **Property 5: Round-trip tone key double-press**
    - **Validates: Requirements 6.1, 6.6**
    - For each âm tiết `[base_keys]` trong corpus rút gọn (50 từ Việt) và mỗi tone key `t` ∈ {s, f, r, x, j} (cộng uppercase): gõ `[base_keys] + t + t` từ trạng thái rỗng, assert output cuối == output của `[base_keys] + t_raw`.

  - [x] 3.6 Implement Property 6 — Round-trip double-key triple-press (`test/test_modifier_round_trip.cpp`)
    - **Property 6: Round-trip double-key triple-press**
    - **Validates: Requirements 6.2**
    - For each `k` ∈ {a, e, o, d} (lower + upper): gõ `k + k + k` từ trạng thái rỗng, assert `_text` cuối == `[k, k]` (2 ký tự thuần).

  - [x] 3.7 Implement Property 7 — Round-trip hook-key double-press (`test/test_modifier_round_trip.cpp`)
    - **Property 7: Round-trip hook-key double-press**
    - **Validates: Requirements 6.3**
    - For each combo tạo hook (`ow→ơ`, `aw→ă`, `uw→ư`, `oow→ô`): gõ thêm `w` lần nữa, assert output cuối là base + `w` raw.

  - [x] 3.8 Implement Property 8 — Tone toggle với phím z (`test/test_modifier_round_trip.cpp`)
    - **Property 8: Tone toggle với phím z**
    - **Validates: Requirements 6.4, 6.5**
    - Case 1: `_text` đã có dấu thanh (gõ `tois` để thành `tói`) → gõ `z` → assert `_text` không còn dấu thanh, không append `z`.
    - Case 2: `_text` không có dấu thanh (gõ `toi`) → gõ `z` → assert `z` được append nguyên dạng.

  - [x] 3.9 Verify Phase 2 — code lặp giảm + tests pass
    - Verify: `grep -c "for (int t = 1; t <= 5;" src/core/CayEngine.cpp` == 0 (mục tiêu Requirement 4.7 ≤ 1, đạt thừa).
    - `cmake --build build && ctest --test-dir build --output-on-failure`: P1-P8 + golden trace P15 đều xanh.
    - `cmake --build build --target cay`: release build vẫn pass (no-CRT).
    - _Requirements: 1.1, 1.4, 4.4, 4.5, 4.6, 4.7_

- [x] 4. Phase 3 — Migrate IsCompleteSyllable lên CayData::TryMatch* API
  - [x] 4.1 Refactor `IsCompleteSyllable` trong `CayEngine.cpp` dùng `CayData::TryMatch*`
    - Xoá toàn bộ 4 array nội bộ `s_initials`, `s_nuclei`, `s_finals`, `s_tails` trong function `IsCompleteSyllable`.
    - Thay logic match bằng 4 call: `CayData::TryMatchInitial(s+pos, len-pos)`, `TryMatchNucleus`, `TryMatchFinal`, `TryMatchTail`.
    - Giữ nguyên: special case "gi" rollback (nếu sau "gi" không phải nguyên âm thì 'i' là nucleus), Nucleus-Final Pairing Rule (nh/ch chỉ đi với a/i/ê/y/oa/uy/uê; ng/c không đi với i/ê/y).
    - Verify: `grep -nE "s_initials|s_nuclei|s_finals|s_tails" src/core/CayEngine.cpp` không có hit (chỉ còn trong CayData.cpp).
    - _Requirements: 2.1, 2.2, 2.3, 2.4_

  - [x] 4.2 Implement Property 9 — Đặt dấu thanh đúng vị trí (`test/test_tone_position.cpp`)
    - **Property 9: Đặt dấu thanh đúng vị trí theo quy tắc tiếng Việt**
    - **Validates: Requirements 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8**
    - Iterate qua 50 từ tiếng Việt trong `g_vietnameseCorpus` đã annotate vị trí dấu kỳ vọng (`expected_tone_pos`).
    - Set engine state với `_text` = từ đã transform (qua test-only friend hoặc gõ chuỗi rồi snapshot), assert `FindTonePosition() == expected_tone_pos`.
    - Bao trùm 7 cases: 1 nguyên âm, 2 nguyên âm + final, 2 nguyên âm mở (oa/oe/uê/uy/uơ/iê), 2 nguyên âm mở khác, 3 nguyên âm + final, 3 nguyên âm mở (uyê/giuô/giươ), qu/gi + nguyên âm.

  - [x] 4.3 Implement Property 10 — Bypass đúng cho corpus (`test/test_bypass.cpp`)
    - **Property 10: Bypass đúng cho corpus tiếng Anh và tiếng Việt**
    - **Validates: Requirements 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7**
    - For each từ Anh trong `g_englishCorpus`: gõ chuỗi → assert `ShouldBypassWord() == true` tại keystroke cuối.
    - For each từ Việt trong `g_vietnameseCorpus`: gõ chuỗi → assert `ShouldBypassWord() == false` tại keystroke cuối (trừ trường hợp Strict Tone-Final Consonant Rule).
    - Cần test-only accessor `bool TelexEngine::DebugShouldBypassWord() const` (gated `#ifdef CAY_TEST_BUILD`) hoặc dùng `friend` declaration.

  - [x] 4.4 Implement Property 13 — Round-trip IsCompleteSyllable với pretty-printer (`test/test_syllable_round_trip.cpp`)
    - **Property 13: Round-trip IsCompleteSyllable với pretty-printer**
    - **Validates: Requirements 17.2, 17.3, 17.4**
    - Helper test `prettyPrintSyllable(initialIdx, nucleusIdx, finalIdx, tailIdx)` ghép 4 thành phần thành chuỗi.
    - Forward direction: enumerate ≥ 200 bộ index hợp lệ → assert `IsCompleteSyllable(s, len) == true` cho chuỗi không vi phạm Pairing Rule.
    - Reverse direction: với corpus chuỗi không hợp lệ (`zzz`, `bbn`, `qz`, ...) → assert `IsCompleteSyllable == false`.
    - Cần expose `IsCompleteSyllable` qua test-only accessor (`#ifdef CAY_TEST_BUILD`) hoặc move thành `CayEngineDebug::IsCompleteSyllable` chỉ trong test build.

  - [x] 4.5 Implement Property 16 — IsValidNucleus đúng trên corpus (`test/test_syllable_round_trip.cpp`)
    - **Property 16: IsValidNucleus đúng trên corpus**
    - **Validates: Requirements 3.4, 3.5, 3.6**
    - For each âm tiết Việt trong corpus: extract phần nucleus (annotation `expected_nucleus` trong corpus), assert `CayData::IsValidNucleus(nucleus, len) == true`.
    - For each chuỗi trong tập đối chiếu `{L"yi", L"yo", L"yu", L"ou", L"\u0103n"}`: assert `IsValidNucleus == false`.
    - Lưu ý: ở Phase 3, `s_nuclei` chưa được dọn → P16 với input `{yi, yo, ...}` sẽ FAIL vì baseline còn entry. Đánh dấu test này `DOCTEST_SKIP` ở Phase 3, bỏ skip ở Phase 4.

  - [x] 4.6 Verify Phase 3 — single source of truth + tests pass
    - Verify: `grep -lE "static const wchar_t\* const s_(initials|nuclei|finals|tails)" src/core/ -r | wc -l` == 1 (chỉ `CayData.cpp`).
    - `cmake --build build && ctest --test-dir build --output-on-failure`: P1-P10, P13 + golden trace P15 đều xanh. P16 skipped.
    - `cmake --build build --target cay`: release build pass.
    - _Requirements: 1.1, 2.1, 2.2, 7.8, 8.7, 17.4_

- [x] 5. Phase 4 — Dọn s_nuclei và s_initials
  - [x] 5.1 Xoá entry `dd` khỏi `s_initials` trong `CayData.cpp`
    - Bỏ literal `L"dd"` trong array `s_initials`. Giữ `L"\u0111"` (đ).
    - Verify: `s_initialsCount` giảm từ 28 xuống 27 (hoặc 26 nếu cũng count trùng `\u0111`).
    - Lý do: `IsCompleteSyllable` được gọi trên `_text` đã transform, tại đó `dd` đã thành `đ`. Entry `dd` là dead.
    - _Requirements: 2.5_

  - [x] 5.2 Xoá entries rác khỏi `s_nuclei` và chuyển sang `\u` escape
    - Xoá: `L"\u0103n"` (ăn — vần có phụ âm cuối, không phải nucleus), `L"yi"`, `L"yo"`, `L"yu"`, `L"ou"`, duplicate `L"\u00e2u"` (chỉ giữ 1).
    - Chuyển TẤT CẢ literal Unicode trong `s_nuclei` sang dạng `\uXXXX` escape (không dùng UTF-8 raw): ví dụ `L"â"` → `L"\u00e2"`, `L"ă"` → `L"\u0103"`, `L"ư"` → `L"\u01b0"`, `L"ơ"` → `L"\u01a1"`.
    - Sắp xếp dài-trước trong array để `TryMatchNucleus` longest-match đúng.
    - Update `s_nucleiCount` (từ 44 xuống ~39).
    - _Requirements: 3.1, 3.2, 3.3, 3.4_

  - [x] 5.3 Bỏ skip Property 16 và verify nuclei đã sạch
    - Bỏ `DOCTEST_SKIP` cho P16 trong `test/test_syllable_round_trip.cpp`.
    - Chạy lại P16: input `yi/yo/yu/ou/ăn` phải trả `IsValidNucleus == false`.
    - Tất cả corpus nucleus còn lại vẫn `IsValidNucleus == true`.
    - _Requirements: 3.5, 3.6_

  - [x] 5.4 Verify Phase 4 — golden trace + corpus
    - `cmake --build build && ctest --test-dir build --output-on-failure`: tất cả P1-P10, P13, P16 + golden trace P15 đều xanh (P15 phải pass — chứng tỏ việc xoá entry rác không thay đổi output cho từ Việt hợp lệ).
    - `cmake --build build --target cay`: release build pass.
    - _Requirements: 1.1, 1.2, 3.4_

- [x] 6. Phase 5 — Unify MAX_BUFFER thành constexpr trong CayTypes.h
  - [x] 6.1 Di chuyển `MAX_BUFFER` từ `#define` sang `constexpr` trong `CayTypes.h`
    - Trong `src/core/CayTypes.h`, thêm `constexpr int MAX_BUFFER = 64;` trong namespace `Cay`.
    - Trong `src/core/CayData.h`, xoá `#define MAX_BUFFER 64`.
    - Verify: tất cả call site `MAX_BUFFER` trong `src/core/CayEngine.h/cpp` và `src/core/CayData.cpp` resolve qua include chain (`CayEngine.h` → `CayData.h` → `CayTypes.h`).
    - Verify: `grep -rE "^#define MAX_BUFFER" src/core/` không có hit.
    - _Requirements: 5.1, 5.2_

  - [x] 6.2 Sửa `MacInputInjector.mm` dùng `Cay::MAX_BUFFER`
    - Trong `src/platform/macos/MacInputInjector.mm`: include `"CayTypes.h"`.
    - Thay `UniChar chars[64]` → `UniChar chars[Cay::MAX_BUFFER]`.
    - Thay loop bound `i < 64` → `i < Cay::MAX_BUFFER`.
    - _Requirements: 5.3, 5.5_

  - [x] 6.3 Sửa `Windows InputInjector.cpp` đủ headroom + comment
    - Trong `src/platform/windows/InputInjector.cpp`: tính lại worst case = 1 dummy + (MAX_BUFFER + 1) backspace pairs + MAX_BUFFER unicode pairs ×2 = 2 + 2×65 + 2×64 = 260 INPUT structs.
    - Đổi `INPUT inputs[256]` → `INPUT inputs[Cay::MAX_BUFFER * 4 + 4]` (= 260 với MAX_BUFFER=64) hoặc giữ literal nhưng comment rõ.
    - Thêm comment header giải thích công thức: `// Worst case: 1 dummy pair + (MAX_BUFFER + 1) backspace pairs + MAX_BUFFER unicode pairs = MAX_BUFFER*4 + 4`.
    - Thay tất cả check `idx + 1 < 256` thành `idx + 1 < Cay::MAX_BUFFER * 4 + 4`.
    - Include `"CayTypes.h"` trong `InputInjector.cpp`.
    - _Requirements: 5.3, 5.4_

  - [x] 6.4 Implement Property 17 — OnInjectText buffer transparency (`test/test_buffer_transparency.cpp`)
    - **Property 17: OnInjectText buffer transparency**
    - **Validates: Requirements 5.4**
    - Generator `genInjectCall`: random `backspaceCount ∈ [0, MAX_BUFFER]`, `newTextLen ∈ [0, MAX_BUFFER]`, `newText` random wchar_t.
    - Mock injector capture call → assert `call.backspaceCount == bs && call.newText.size() == newTextLen && call.newText[i] == newText[i]`.
    - Iterations: 200.

  - [x] 6.5 Verify Phase 5 — build all 3 platform pass
    - `cmake -B build -DBUILD_TESTING=ON && cmake --build build`: macOS bundle pass.
    - Cross-check Windows config: `cmake --preset windows` (nếu có) hoặc verify bằng `static_assert(Cay::MAX_BUFFER == 64);` trong `src/core/CayEngine.cpp` để ngăn hồi quy.
    - Cross-check Fcitx5 config: `cmake -DBUILD_FCITX5=ON -B build_fcitx5 && cmake --build build_fcitx5` (nếu môi trường có sẵn fcitx5 dev).
    - `ctest --test-dir build --output-on-failure`: P1-P10, P13, P16, P17 + golden trace P15 đều xanh.
    - _Requirements: 1.4, 5.1, 5.2, 5.3, 11.5_

- [x] 7. Phase 6 — Fix mojibake comments + polish
  - [x] 7.1 Verify và đảm bảo `src/core/*.cpp/.h` là UTF-8 không BOM
    - Sử dụng `file src/core/*.cpp src/core/*.h` để verify encoding.
    - Nếu có BOM (3 byte `EF BB BF` đầu file): mở bằng editor và save lại "UTF-8 without BOM".
    - Lưu ý: cẩn thận với MSVC `/utf-8` flag — đã có sẵn trong project (`CMakeLists.txt`).
    - _Requirements: 15.1_

  - [x] 7.2 Sửa mojibake trong comments của `src/core/CayEngine.cpp`
    - Sửa tất cả chuỗi vỡ tiếng Việt: `Lu?t Q` → `Luật Q`, `B?t bu?c di v?i u` → `Bắt buộc đi với u`, `B? qua c�c t? mu?n nhu pin` → `Bỏ qua các từ mượn như pin`, `T?I ��Y` → `TẠI ĐÂY`, `Ti?ng Vi?t` → `Tiếng Việt`, `Ph? �m k�p` → `Phụ âm kép`, etc.
    - Đảm bảo: comment phản ánh đúng ý đồ ban đầu (không tự bịa). Nếu không đoán được mojibake gốc, viết lại comment ngắn gọn bằng tiếng Việt UTF-8 đúng nghĩa.
    - Verify: `grep -n "?" src/core/CayEngine.cpp` chỉ tìm thấy ternary/regex (không phải comment vỡ).
    - _Requirements: 15.2, 15.3_

  - [x] 7.3 Chuyển terminal list trong `CayimeEngine.cpp` thành `g_terminalProgramNames` const array
    - Trong `src/platform/fcitx5/CayimeEngine.cpp`: tạo `static const std::string_view g_terminalProgramNames[] = { "terminal", "alacritty", "kitty", "konsole", "terminator", "wezterm", "tmux" };` ở đầu file.
    - Thay chain `prog.find("terminal") != npos || prog.find("alacritty") != npos || ...` bằng for-loop duyệt array.
    - Thêm comment giải thích: `// Forced fallback to BackSpace forwarding because VTE-based terminals report SurroundingText capability but ignore deleteSurroundingText (known bug).`
    - _Requirements: 16.1, 16.3, 16.4_

  - [x] 7.4 Thêm comment header vào `src/core/*.h` documenting single source of truth + no-CRT
    - Vào đầu mỗi file `CayTypes.h`, `CayData.h`, `CayEngine.h`: thêm comment block 10-15 dòng theo template trong design.
    - Comment ghi rõ: (a) bảng nào sống ở đâu, (b) ràng buộc no-CRT/no-allocation/no-exception, (c) API chính.
    - _Requirements: 19.2_

  - [x] 7.5 Verify Phase 6 — smoke checks pass
    - `bash test/check_invariants.sh`: tất cả 5 check xanh (single source, lặp ≤ 1, MAX_BUFFER constexpr, no STL, UTF-8 no BOM).
    - `cmake --build build && ctest --test-dir build --output-on-failure`: tất cả tests pass.
    - `cmake --build build --target cay`: release build pass.
    - _Requirements: 2.1, 4.7, 5.1, 11.1, 15.1, 15.2, 15.3, 16.3_

- [x] 8. Phase 7 — Cleanup dead code và đo binary size
  - [x] 8.1 Audit dead code và xoá hoặc document
    - `grep -rn "CayData::IsValidInitial\|CayData::IsValidNucleus" src/ test/` — verify call sites.
    - Nếu sau Phase 3 không còn caller trong `src/core/` cho `IsValidNucleus` và chỉ còn dùng trong test: thêm comment ` // Public utility — used by tests only after migration to TryMatchNucleus.` lên khai báo trong `CayData.h`.
    - Cho `OnKeyUp`: thêm comment `// Required by platform contract (Windows KeyboardHookManager, MacHookManager forward keyup events). Currently no-op.` lên khai báo trong `CayEngine.h`.
    - Verify: không có hàm public nào không có call site và không có comment giải thích.
    - _Requirements: 14.1, 14.2, 14.4_

  - [x] 8.2 Implement Property 11 — Idempotency của StripAllTones và ResetState (`test/test_idempotency.cpp`)
    - **Property 11: Idempotency của các reset operations**
    - **Validates: Requirements 10.1, 10.2**
    - Cần test-only accessor `TelexEngine::DebugStripAllTones()`, `DebugResetState()` (gated `#ifdef CAY_TEST_BUILD`).
    - For each chuỗi key sequence ngẫu nhiên (100 iterations): gõ → snapshot `_text` → gọi `StripAllTones()` lần 1 → snapshot → gọi lần 2 → snapshot → assert snapshot 1 == snapshot 2.
    - Tương tự cho `ResetState()`: snapshot toàn bộ field nội bộ qua `GetDebugState()`.

  - [x] 8.3 Implement Property 12 — ResetFull tương đương khởi tạo mới (`test/test_idempotency.cpp`)
    - **Property 12: ResetFull tương đương khởi tạo mới**
    - **Validates: Requirements 10.3**
    - Construct `TelexEngine e1, e2`. Gõ random sequence trên `e1` → gọi `e1.ResetFull()` → assert `e1.GetDebugState() == e2.GetDebugState()` (qua `memcmp` của struct hoặc field-by-field).

  - [x] 8.4 Implement Property 14 — Space + Backspace round-trip (`test/test_canrestore.cpp`)
    - **Property 14: Space + Backspace round-trip**
    - **Validates: Requirements 18.1, 18.2, 18.3**
    - Generator `genTelexSyllable()` sinh chuỗi key tạo âm tiết Việt hợp lệ (có `_textLen > 0`).
    - For each chuỗi: gõ chuỗi → snapshot state S1 → gõ Space → gõ Backspace → snapshot S2 → assert S1.text == S2.text, S1.toneIndex == S2.toneIndex, S2.canRestore == false.
    - Iterations: 100.

  - [x] 8.5 Tạo `test/check_size.sh` đo binary size before/after
    - Shell script đọc kích thước binary `build/cay`, `build/cay.app/Contents/MacOS/cay`, `build/src/platform/fcitx5/libcayime.so` (nếu BUILD_FCITX5=ON), in ra stdout.
    - Format output: `Platform | Binary | Size (bytes) | Size (KB)`.
    - Compare với baseline (ghi tại Phase 0 vào `test/baseline_size.txt` qua subcommand `--record-baseline`).
    - Exit code khác 0 nếu binary size > 110% baseline (cho phép tăng tối đa 10% theo Requirement 12).
    - _Requirements: 12.1, 12.2, 12.3, 12.4_

  - [x] 8.6 Record baseline size tại Phase 0 retroactively và verify Phase 7
    - Nếu chưa có `test/baseline_size.txt`: checkout commit ngay trước Phase 0, build release, chạy `bash test/check_size.sh --record-baseline`, commit file.
    - Tại HEAD (sau Phase 7): build release trên cả 3 platform khả dụng (macOS chắc chắn, Windows/Fcitx5 nếu có CI).
    - Chạy `bash test/check_size.sh` — assert output ≤ 110% baseline cho mọi platform.
    - Ghi kết quả số đo (before/after KB cho mỗi platform) vào "Notes" section của tasks.md này (thay cho `docs/refactor-size-report.md` để tránh tạo doc/markdown ngoài spec — theo user rule).
    - _Requirements: 12.1, 12.2, 12.3, 12.4_

  - [x] 8.7 Verify final — toàn bộ test suite + smoke + golden xanh
    - `bash test/check_invariants.sh`: tất cả 5 check xanh.
    - `cmake --build build --target cay_test && ctest --test-dir build --output-on-failure`: tất cả 17 properties + golden trace P15 đều xanh.
    - `cmake --build build --target cay`: release build pass với `/EHs-c-`, `/GR-`, `/NODEFAULTLIB:msvcrt.lib` (verified qua CMakeLists.txt — không thay đổi).
    - `bash test/check_size.sh`: binary size ≤ 110% baseline trên mọi platform.
    - `git grep -nE "for \(int t = 1; t <= 5;" src/core/CayEngine.cpp`: 0 hit (Requirement 4.7).
    - `git grep -nE "static const wchar_t\\* const s_(initials|nuclei|finals|tails)" src/core/`: chỉ trong `CayData.cpp`.
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 2.1, 4.7, 11.5, 12.1, 12.2, 12.3_

## Notes

### Quy ước test
- Mọi file C++ test nằm trong `test/`, đặt tên `test_<scope>.cpp`. File check bash đặt tên `check_<scope>.sh`.
- Test target `cay_test` được gated bởi `BUILD_TESTING=ON` — KHÔNG ảnh hưởng release binary (no-CRT preserved).
- Test build dùng STL tự do (`<vector>`, `<string>`, `<random>`, ...) vì test binary tách riêng. Tuy nhiên KHÔNG được include STL trong bất kỳ file nào dưới `src/core/`.
- Mỗi `TEST_CASE` có comment tag `// [Feature: engine-core-refactor, Property N: <text>]` để truy ngược về spec.

### Quy ước implementation
- Sub-tasks postfix `*` là optional (test scaffolding) — có thể skip nếu cần MVP nhanh.
- Mỗi task sau khi hoàn thành: `cmake --build build && ctest --test-dir build` phải xanh. Nếu không xanh: rollback, debug, không skip.
- Golden trace P15 phải xanh sau mọi phase ≥ Phase 1 — đây là gate Behavioral_Equivalence.
- Smoke check `test/check_invariants.sh` phải xanh sau Phase 6 (Phase 0–5 có thể fail một số check, được expected).

### Nguyên tắc no-CRT trong refactor
- Không include `<vector>`, `<string>`, `<algorithm>`, `<map>`, `<unordered_map>`, `<memory>`, `<chrono>`, `<random>` trong `src/core/`.
- Không gọi `new`, `malloc`, `delete`, `free`. Không cấp phát động trong Hot_Path.
- Không `throw`, không `try/catch`. Không RTTI.
- Build Windows release phải link với `/NODEFAULTLIB:msvcrt.lib` thành công (giữ nguyên flag từ CMakeLists.txt).

### Binary size baseline (đo retroactively cho task 8.6)
| Platform | Baseline (bytes / KB) | After refactor (bytes / KB) | Delta | Status |
|---|---|---|---|---|
| macOS `cay.app/Contents/MacOS/cay` (universal arm64+x86_64, ad-hoc codesigned) | 228560 / 223.2 | 231680 / 226.2 | +3120 B / +1.4% | PASS (≤ 110%) |
| Windows `cay.exe` | not measured | not measured | n/a | pending CI |
| Linux `libcayime.so` (Fcitx5) | not measured | not measured | n/a | pending CI |

Mục tiêu: ≤ 110% baseline (Requirement 12). Lý tưởng: ≤ 100%.

#### Binary size measurements (task 8.6 — Phase 7)
- **Pre-Phase-0 commit**: `0b3112f` — *"Fix terminal bug by blacklisting buggy terminal emulators from SurroundingText"* (tag `v1.0.1`).
- **Build pipeline (cả baseline và HEAD)**:
  ```
  cmake -B <dir> -S <src> -DCMAKE_BUILD_TYPE=Release
  cmake --build <dir> --target cay
  codesign --force --sign - --timestamp=none <dir>/cay.app   # ad-hoc, khớp với CMake POST_BUILD step
  ```
- **Host**: macOS, AppleClang 21.0.0.21000101, universal binary `arm64;x86_64` (per `CMAKE_OSX_ARCHITECTURES` trong `CMakeLists.txt`).
- **Cách đo**: `stat -f %z build/cay.app/Contents/MacOS/cay`.
- **Lưu ý apple-to-apple**: HEAD `CMakeLists.txt` có `add_custom_command(POST_BUILD ... codesign ...)` (commit `cc3c65c` trở đi). Phần `LINKEDIT` (chứa code signature) đóng góp ~17 KB cho universal binary, nên baseline được ad-hoc codesign cùng cách trước khi đo.
- **Baseline `test/baseline_size.txt`** đã commit chỉ chứa entry `macOS|build/cay.app/Contents/MacOS/cay|228560`. Windows/Fcitx5 entries sẽ được ghi vào file này khi có CI runner cho từng platform — `test/check_size.sh` được thiết kế để skip nếu binary path không tồn tại, không yêu cầu mọi platform phải có sẵn cùng lúc.
- **Kết quả `bash test/check_size.sh`**: `PASS` (exit 0). macOS binary 231680 B = 101.4% baseline (228560 B), tăng 1.4% — trong ngưỡng 110% theo Requirement 12.

### Mapping Property → Test File → Requirements
| Property | File | Requirements |
|---|---|---|
| P1 Round-trip Decompose/Compose | `test_decompose_compose.cpp` | 4.1, 4.2, 4.3 |
| P2 Round-trip StripTone/GetToneMark | `test_decompose_compose.cpp` | 9.1, 9.5 |
| P3 Completeness GetToneMark | `test_decompose_compose.cpp` | 9.3 |
| P4 GetToneIndex inverse | `test_decompose_compose.cpp` | 9.4 |
| P5 Tone key double-press | `test_modifier_round_trip.cpp` | 6.1, 6.6 |
| P6 Double-key triple-press | `test_modifier_round_trip.cpp` | 6.2 |
| P7 Hook-key double-press | `test_modifier_round_trip.cpp` | 6.3 |
| P8 Tone toggle z | `test_modifier_round_trip.cpp` | 6.4, 6.5 |
| P9 Tone position | `test_tone_position.cpp` | 7.1–7.8 |
| P10 Bypass corpus | `test_bypass.cpp` | 8.1–8.7 |
| P11 Idempotency reset | `test_idempotency.cpp` | 10.1, 10.2 |
| P12 ResetFull == new | `test_idempotency.cpp` | 10.3 |
| P13 Round-trip syllable | `test_syllable_round_trip.cpp` | 17.2, 17.3, 17.4 |
| P14 Space+Backspace | `test_canrestore.cpp` | 18.1, 18.2, 18.3 |
| P15 Golden trace | `golden_trace.cpp` | 1.1, 1.2, 1.3, 7.8, 8.7, 18.3 |
| P16 IsValidNucleus | `test_syllable_round_trip.cpp` | 3.4, 3.5, 3.6 |
| P17 Buffer transparency | `test_buffer_transparency.cpp` | 5.4 |

## Task Dependency Graph

```json
{
  "waves": [
    { "id": 0, "tasks": ["1.1", "1.2", "1.3", "1.4", "1.7"] },
    { "id": 1, "tasks": ["1.5"] },
    { "id": 2, "tasks": ["1.6"] },
    { "id": 3, "tasks": ["1.8"] },
    { "id": 4, "tasks": ["2.1", "2.3"] },
    { "id": 5, "tasks": ["2.2"] },
    { "id": 6, "tasks": ["2.4", "2.5", "2.6", "2.7"] },
    { "id": 7, "tasks": ["2.8"] },
    { "id": 8, "tasks": ["3.1", "3.2", "3.3"] },
    { "id": 9, "tasks": ["3.4"] },
    { "id": 10, "tasks": ["3.5", "3.6", "3.7", "3.8"] },
    { "id": 11, "tasks": ["3.9"] },
    { "id": 12, "tasks": ["4.1"] },
    { "id": 13, "tasks": ["4.2", "4.3", "4.4", "4.5"] },
    { "id": 14, "tasks": ["4.6"] },
    { "id": 15, "tasks": ["5.1", "5.2"] },
    { "id": 16, "tasks": ["5.3"] },
    { "id": 17, "tasks": ["5.4"] },
    { "id": 18, "tasks": ["6.1"] },
    { "id": 19, "tasks": ["6.2", "6.3"] },
    { "id": 20, "tasks": ["6.4"] },
    { "id": 21, "tasks": ["6.5"] },
    { "id": 22, "tasks": ["7.1"] },
    { "id": 23, "tasks": ["7.2", "7.3", "7.4"] },
    { "id": 24, "tasks": ["7.5"] },
    { "id": 25, "tasks": ["8.1"] },
    { "id": 26, "tasks": ["8.2", "8.3", "8.4", "8.5"] },
    { "id": 27, "tasks": ["8.6"] },
    { "id": 28, "tasks": ["8.7"] }
  ]
}
```
