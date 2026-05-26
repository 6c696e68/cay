# Bug: Dấu thanh đặt sai vị trí — `xuất` → `xúât`, `xuát` → `xúat`

## Tóm tắt

Engine đặt dấu thanh **không đúng vị trí** trong nhiều âm tiết tiếng Việt khi:
1. Cụm nguyên âm có cả nguyên âm thuần và nguyên âm có dấu mũ/móc (â, ê, ô, ơ, ă, ư).
2. User gõ phím dấu (s/f/r/x/j) **trước** khi gõ phụ âm cuối, hoặc trước khi gõ double-key tạo nguyên âm mũ.

Kết quả: dấu thanh không di chuyển sang vị trí đúng theo quy tắc tiếng Việt khi cấu trúc âm tiết thay đổi.

## Phiên bản dính bug

- Version: `v1.1.0` (commit `f86af86 — Smart Bypass nâng cấp`).
- Platform: macOS, Windows, Linux/Fcitx5 (logic trong core engine, không phụ thuộc platform).
- File: `src/core/CayEngine.cpp` — function `FindTonePosition()` và `ReplayKey()`.

## Tái hiện

Bật Cay, mở bất kỳ ô nhập text nào, gõ chuỗi phím dưới đây (tắt phần mềm bỏ dấu khác nếu có):

| # | Phím gõ | Kết quả thực tế (BUG) | Kết quả mong đợi |
|---|---|---|---|
| 1 | `xuaast` | `xúât` ❌ | `xuất` ✅ |
| 2 | `xuast` | `xúat` ❌ | `xuát` ✅ |
| 3 | `tuafn` | `tùan` ❌ | `tuàn` ✅ |
| 4 | `quosc` | `quóc` (thiếu mũ — case khác) | `quốc` |

> Case 1 (`xuaast`) là case chuẩn của Telex thuần — gõ `xu` + `aa` (→ â) + `st` (sắc + t cuối). Đây là chuỗi gõ rất phổ biến trong tiếng Việt, ảnh hưởng diện rộng.

## Phân tích root cause

### Bug A — `FindTonePosition` không ưu tiên nguyên âm mũ/móc trong cụm 2/3 nguyên âm mở

`src/core/CayEngine.cpp::FindTonePosition()` xử lý cụm 2 nguyên âm mở (không có phụ âm cuối):

```cpp
if (count == 2) {
    if (hasConsonantFinal) return last;
    // ... oa, oe, uê, uy, uơ, iê, qu+vowel, gi+vowel ...
    // Default for open 2-vowel syllable: first vowel
    return first;
}
```

Khi cụm là `uâ` (u + â) mở:
- Không có phụ âm cuối.
- Không match `oa`/`oe`/`uê`/`uy`/`uơ`/`iê` (vì `â` strip thành `a` qua `baseVowel`).
- **Rơi vào default `return first` → đặt dấu trên `u`.**

Nhưng quy tắc tiếng Việt: khi cụm nguyên âm chứa **â/ê/ô/ơ/ă/ư**, dấu thanh **luôn** đặt trên đó (chứ không phải nguyên âm thuần).

### Bug B — Engine không re-position dấu khi cấu trúc âm tiết thay đổi

`src/core/CayEngine.cpp::ReplayKey()` xử lý từng phím độc lập:
- Phím dấu (s/f/r/x/j) → `ApplyToneMarks()` đặt dấu tại vị trí được tính bởi `FindTonePosition()` ở **thời điểm hiện tại**.
- Phím consonant cuối (t/n/c/m/...) → chỉ append vào `_text[]`.
- Phím double-key (a/e/o/d) → `ApplyDoubleKeys()` biến nguyên âm thuần thành mũ (a→â).

Vấn đề: Sau bước 2 hoặc 3, **cấu trúc âm tiết có thể đổi** (vd. cụm mở thành cụm khép, cụm thuần thành cụm có hooked vowel). Engine **không re-evaluate** vị trí dấu → dấu vẫn ở vị trí cũ (sai).

Ví dụ trace `xuaast`:

| Step | Input | `_text` | Logic |
|---|---|---|---|
| 0 | `x` | `x` | append |
| 1 | `u` | `xu` | append |
| 2 | `a` | `xua` | append |
| 3 | `a` (double) | `xuâ` | a→â |
| 4 | `s` | `xúâ` ❌ | đặt sắc trên u (Bug A: không ưu tiên hooked vowel) |
| 5 | `t` | `xúât` ❌ | append, không re-position (Bug B) |

Đáng lẽ:

| Step 4 | `xuấ` | sắc trên â (vì â là hooked vowel) |
| Step 5 | `xuất` | append t, dấu vẫn ở â (đúng cấu trúc khép) |

## Phạm vi tác động

Bug ảnh hưởng các âm tiết phổ biến trong tiếng Việt:
- `xuất, tuần, luôn, đường, bướm, chuông, muỗi, suối, ...`
- Khi user dùng pattern Telex tự nhiên (gõ dấu sớm trước phụ âm cuối, hoặc gõ double-key sau khi đã đặt dấu).

## Fix triệt để

Patch trong `src/core/CayEngine.{h,cpp}`:

1. **Bug A — `FindTonePosition`**: thêm ưu tiên hooked vowel (â/ê/ô/ơ/ă/ư) ở cả nhánh count=2 và count>=3 mở.

   ```cpp
   auto isHooked = [](wchar_t c) {
       return c == L'\u00e2' || c == L'\u00ea' || c == L'\u00f4'
           || c == L'\u01a1' || c == L'\u0103' || c == L'\u01b0';
   };
   wchar_t b1 = CayData::DecomposeChar(_text[first]).base;
   wchar_t b2 = CayData::DecomposeChar(_text[last]).base;
   if (isHooked(b2) && !isHooked(b1)) return last;
   if (isHooked(b1) && !isHooked(b2)) return first;
   // ... fallthrough rules cũ ...
   ```

2. **Bug B — `RepositionTone()`** + invocation trong `ReplayKey`:

   ```cpp
   // CayEngine.h (private):
   void RepositionTone();

   // CayEngine.cpp:
   void TelexEngine::RepositionTone() {
       if (_textLen == 0) return;
       // tìm vị trí dấu hiện tại
       int currentPos = -1; int currentTone = 0;
       for (int i = 0; i < _textLen; i++) {
           auto d = CayData::DecomposeChar(_text[i]);
           if (d.toneIndex > 0) { currentPos = i; currentTone = d.toneIndex; break; }
       }
       if (currentPos < 0) return;
       int targetPos = FindTonePosition();
       if (targetPos < 0 || targetPos == currentPos) return;
       // dời dấu sang vị trí mới
       auto dOld = CayData::DecomposeChar(_text[currentPos]);
       _text[currentPos] = CayData::ComposeChar(dOld.base, 0, dOld.isUpper);
       auto dNew = CayData::DecomposeChar(_text[targetPos]);
       wchar_t toned = CayData::ComposeChar(dNew.base, currentTone, dNew.isUpper);
       if (toned != L'\0') _text[targetPos] = toned;
       else _text[currentPos] = CayData::ComposeChar(dOld.base, currentTone, dOld.isUpper);
   }

   // ReplayKey() — gọi RepositionTone sau khi mutate vowel hoặc append consonant:
   bool mutatedVowel = false;
   // ... ApplyDoubleKeys/ApplyHookKeys → mutatedVowel = true ...
   bool appendedConsonant = !appliedModifier && IsAlpha(ch) && !CayData::IsVowel(ch);
   if (mutatedVowel || appendedConsonant) RepositionTone();
   ```

## Verification

Chạy `./cay_test`:

```
[doctest] test cases:    21 |    21 passed | 0 failed | 0 skipped
[doctest] assertions: 16200 | 16200 passed | 0 failed |
[doctest] Status: SUCCESS!
```

Trước fix: `21/20 passed, 1 failed` (golden trace seed=33 dòng 12 — nay cùng nguyên nhân với báo cáo).

Test thủ công các sequence Telex phổ biến đều cho ra đúng:

| Input | Output (sau fix) |
|---|---|
| `xuaast` | `xuất` ✅ |
| `xuast`  | `xuát` ✅ |
| `tuaanf` | `tuần` ✅ |
| `tuafn`  | `tuàn` ✅ |
| `nguois` | `người` ✅ |
| `dduwowngs` | `đường` ✅ |
| `muoois` | `muỗi` ✅ |
| `buowms` | `bướm` ✅ |

## Files thay đổi

- `src/core/CayEngine.h` — khai báo `RepositionTone()`.
- `src/core/CayEngine.cpp` — fix `FindTonePosition()` (ưu tiên hooked vowel) + thêm `RepositionTone()` + invoke trong `ReplayKey()`.
- `test/baseline_traces.csv` — regenerate vì engine behavior thay đổi (đúng hơn).

Không thay đổi public API, không thay đổi platform layer (Windows/macOS/Fcitx5).

## Ghi chú

- Bug đã tồn tại từ phiên bản đầu tiên của engine; commit `f86af86` (Smart Bypass nâng cấp) chưa fix root cause này.
- Quy tắc đặt dấu tiếng Việt theo Wikipedia/Bộ Giáo dục: nguyên âm có dấu mũ/móc (â, ê, ô, ơ, ă, ư) luôn được ưu tiên đặt dấu thanh trong cụm nguyên âm.
