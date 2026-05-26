#pragma once
// ============================================================================
// CayData.h — Single Source of Truth cho bảng âm tiết tiếng Việt
//
// Bảng (định nghĩa duy nhất tại `src/core/CayData.cpp`):
//   - s_initials   (cụm phụ âm đầu)        - s_nuclei  (nhân nguyên âm)
//   - s_finals     (phụ âm cuối)           - s_tails   (vần phụ)
//
// API chính:
//   - DecomposeChar / ComposeChar           (Property 1 — round-trip ký tự)
//   - StripTone / GetToneMark / GetToneIndex (xử lý dấu thanh)
//   - ToLowerViet / ToUpperViet              (case-fold tiếng Việt)
//   - TryMatchInitial/Nucleus/Final/Tail     (longest-prefix match — Property 13)
//
// Ràng buộc:
//   * No-CRT      — không include <cstring>/<cwchar>; helper trong CayTypes.h.
//   * No-alloc    — bảng nằm trong .rdata (static const), API stateless.
//   * No-except   — không throw; sentinel 0/-1 cho fail-fast.
// ============================================================================

#include "CayTypes.h"

namespace Cay {

// ---------------------------------------------------------------------------
// Chỉ số dấu (dùng làm index array trong toàn bộ engine).
//   0 = không dấu (ngang)
//   1 = huyền  (f)
//   2 = sắc    (s)
//   3 = hỏi    (r)
//   4 = ngã    (x)
//   5 = nặng   (j)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// DecomposedChar
//
// Kết quả phân rã 1 ký tự nguyên âm có dấu thành 3 thành phần:
//   - base       : nguyên âm cơ bản đã StripTone, viết thường
//                  (thuần a/e/i/o/u/y hoặc đã có dấu mũ/móc â/ă/ê/ô/ơ/ư).
//   - toneIndex  : chỉ số dấu thanh 0..5 (0 = không dấu).
//   - isUpper    : true nếu ký tự gốc là chữ hoa.
//
// Trivial POD — không constructor, không cấp phát động. Pass by value.
// ---------------------------------------------------------------------------
struct DecomposedChar {
    wchar_t base;
    int     toneIndex;
    bool    isUpper;
};

// ---------------------------------------------------------------------------
// CayData
//
// Class helper static thuần túy: bảng validation và map ký tự có dấu.
// Không có instance, không cấp phát động, không dùng CRT.
// ---------------------------------------------------------------------------
class CayData {
public:
    // Trả về true nếu chuỗi `s` (đã null-terminated) với độ dài `len`
    // là một cụm phụ âm đầu tiếng Việt hợp lệ.
    // Public utility — `IsCompleteSyllable` đã chuyển sang `TryMatchInitial`
    // (longest-prefix-match) ở Phase 3 và không còn caller trong `src/core/`.
    // Giữ public API để platform layer / future external integration có thể
    // validate cụm phụ âm đầu mà không cần lấy độ dài match.
    static bool IsValidInitial(const wchar_t* s, int len);

    // Trả về true nếu chuỗi `s` (đã null-terminated) với độ dài `len`
    // là một nguyên âm tiếng Việt hợp lệ.
    // Public utility — used by tests only after migration to TryMatchNucleus.
    // (`IsCompleteSyllable` đã chuyển sang `TryMatchNucleus` ở Phase 3; chỉ
    //  còn `test/test_syllable_round_trip.cpp` Property 16 dùng API này để
    //  verify single-source-of-truth nuclei table.)
    static bool IsValidNucleus(const wchar_t* s, int len);

    // Map phím Telex modifier sang chỉ số dấu (0–5).
    // Trả về -1 nếu phím không phải là phím dấu.
    static int  GetToneIndex(wchar_t key);

    // Trả về codepoint có dấu cho nguyên âm cơ bản + chỉ số dấu.
    // Trả về 0 nếu không có mapping.
    static wchar_t GetToneMark(wchar_t base, int toneIndex);

    // Trả về true nếu `ch` là ký tự tiếng Việt có dấu
    // (đã có dấu mũ, dấu hỏi, dấu ngắn hoặc dấu thanh).
    static bool HasVietnameseMark(wchar_t ch);

    // Trả về true nếu bất kỳ ký tự nào trong `buf[0..len)` có dấu tiếng Việt.
    static bool HasVietnameseMark(const wchar_t* buf, int len);

    // Bỏ dấu thanh từ nguyên âm, trả về nguyên âm ASCII thuần (a/e/i/o/u/y).
    // Trả về `ch` không đổi nếu không phải nguyên âm có dấu.
    static wchar_t StripTone(wchar_t ch);

    // Bỏ dấu mũ/dấu hỏi/dấu ngắn từ nguyên âm, trả về nguyên âm ASCII thuần.
    // Trả về `ch` không đổi nếu không có dấu nào.
    static wchar_t StripAccent(wchar_t ch);

    // Trả về true nếu `ch` là bất kỳ dạng nào của nguyên âm tiếng Việt (thuần hoặc có dấu).
    static bool IsVowel(wchar_t ch);

    // Lấy quy tắc dấu mũ cho nguyên âm
    static wchar_t GetHookRule(wchar_t c);

    // Chuyển ký tự `c` về chữ thường (tiếng Việt + Latin-1 + Vietnamese precomposed).
    // Trả về `c` không đổi nếu không phải chữ hoa hoặc không nằm trong các dải hỗ trợ.
    static wchar_t ToLowerViet(wchar_t c);

    // Chuyển ký tự `c` về chữ hoa (tiếng Việt + Latin-1 + Vietnamese precomposed).
    // Trả về `c` không đổi nếu không phải chữ thường hoặc không nằm trong các dải hỗ trợ.
    static wchar_t ToUpperViet(wchar_t c);

    // -----------------------------------------------------------------------
    // Decompose / Compose — phân rã & tái tạo ký tự nguyên âm có dấu.
    //
    // `DecomposeChar(c)` tách `c` thành `(base, toneIndex, isUpper)` trong đó:
    //   - `base`      : nguyên âm đã StripTone, viết thường (giữ dấu mũ/móc).
    //   - `toneIndex` : chỉ số dấu thanh 0..5 (0 nếu `c` không có dấu thanh).
    //   - `isUpper`   : true nếu `c` là chữ hoa (`c != ToLowerViet(c)`).
    //
    // `ComposeChar(base, toneIndex, isUpper)` tái tạo ký tự gốc:
    //   - `toneIndex == 0` → `base` (đã chuyển hoa nếu `isUpper`).
    //   - `toneIndex 1..5` → `GetToneMark(base, toneIndex)` (đã chuyển hoa).
    //
    // Round-trip property (Requirement 4.3):
    //   FOR ALL ký tự `c` mà CayData hỗ trợ:
    //     auto d = DecomposeChar(c);
    //     ComposeChar(d.base, d.toneIndex, d.isUpper) == c
    //
    // Complexity: O(1) — không vòng for, không cấp phát, không gọi CRT.
    // Mục đích: dùng trong `ApplyDoubleKeys` / `ApplyHookKeys` / `ApplyToneMarks`
    // để loại bỏ pattern lặp `for (int t = 1; t <= 5; t++)` (Requirement 4.4-4.7).
    // -----------------------------------------------------------------------
    static DecomposedChar DecomposeChar(wchar_t c);
    static wchar_t        ComposeChar(wchar_t base, int toneIndex, bool isUpper);

    // -----------------------------------------------------------------------
    // Match Tables — Longest-prefix match trên 4 bảng âm tiết.
    //
    // Mỗi helper duyệt bảng tương ứng (`s_initials` / `s_nuclei` / `s_finals`
    // / `s_tails`) đã được sắp xếp DÀI-TRƯỚC, so khớp prefix `s[0..entryLen)`
    // theo từng `wchar_t`. Trả về số `wchar_t` của entry match đầu tiên
    // (chính là độ dài match dài nhất, do bảng đã sắp dài-trước). Trả `0`
    // nếu không có entry nào match (sentinel — không phải lỗi).
    //
    // Tham số:
    //   - `s`   : con trỏ tới ký tự đầu cần khớp (không cần null-terminated).
    //   - `len` : số `wchar_t` còn lại trong buffer kể từ `s`. `<= 0` → trả 0.
    //
    // Complexity: O(N) với N = số entry trong bảng (≤ 30).
    //
    // Caller dự kiến: `CayEngine::IsCompleteSyllable` ở Phase 3 dùng pattern
    //   `pos += CayData::TryMatchInitial(s + pos, len - pos);`
    // để bóc lần lượt 4 block initial / nucleus / final / tail.
    //
    // Lưu ý: `TryMatchNucleus` yêu cầu caller đã chuẩn hoá `s` về dạng
    // tone-stripped (chỉ còn dấu mũ/móc, không có dấu thanh) — bảng nuclei
    // không chứa nguyên âm có dấu thanh.
    // -----------------------------------------------------------------------
    static int TryMatchInitial(const wchar_t* s, int len);
    static int TryMatchNucleus(const wchar_t* s, int len);
    static int TryMatchFinal  (const wchar_t* s, int len);
    static int TryMatchTail   (const wchar_t* s, int len);
};

} // namespace Cay
