#include "CayData.h"

// ============================================================================
// LƯU Ý: Tất cả array bên dưới được khai báo `static const` để nằm trong
// segment read-only (.rdata) và được khởi tạo hoàn toàn trước khi wWinMain
// được gọi – quan trọng cho build No-CRT (QUY TẮC 2).
// ============================================================================

namespace Cay {

// ---------------------------------------------------------------------------
// PHẦN 1: Bảng validation phụ âm đầu
// 27 cụm phụ âm đầu tiếng Việt được nhận biết (dạng Telex sau transform).
// Sắp xếp DÀI-TRƯỚC để `TryMatchInitial` longest-prefix match đúng
// (vd. "ngh" trước "ng", "ng" trước "n").
//
// Lý do không có "dd": `IsCompleteSyllable` luôn được gọi trên `_text` đã
// transform — tại đó "dd" đã thành "đ" (U+0111). Entry "dd" trong bảng
// initials là dead code (đã xoá ở Phase 4 task 5.1).
// ---------------------------------------------------------------------------
static const wchar_t* const s_initials[] = {
    // Length 3
    L"ngh",
    // Length 2
    L"ch", L"gh", L"gi", L"kh", L"ng", L"nh", L"ph", L"qu", L"th", L"tr",
    // Length 1
    L"b", L"c", L"d", L"\u0111", L"g", L"h", L"k", L"l", L"m", L"n",
    L"p", L"r", L"s", L"t", L"v", L"x"
};
static const int s_initialsCount = sizeof(s_initials) / sizeof(s_initials[0]);

// ---------------------------------------------------------------------------
// PHẦN 2: Bảng validation nguyên âm (nuclei)
// Sắp xếp DÀI-TRƯỚC để `TryMatchNucleus` longest-prefix match đúng
// (vd. "iêu" trước "iê" trước "i").
//
// Bảng này là single source of truth cho nucleus (đã migrate khỏi
// `IsCompleteSyllable` ở Phase 3 task 4.1). Giữ các tổ hợp ASCII Telex
// (ieu, yeu, uoi, uou, oao, oeo, uyu, uye) song song với các literal có
// dấu (đã transform) để cover cả 2 trạng thái buffer.
//
// Convention encoding: TẤT CẢ literal Unicode dùng `\uXXXX` escape (không
// dùng UTF-8 raw) — an toàn với mọi compiler / locale, không bị mojibake.
//
// Đã dọn rác (task 5.2):
//   - Xoá entry `\u0103n` (ăn — vần có phụ âm cuối, không phải nucleus)
//   - Xoá tổ hợp không hợp lệ tiếng Việt: `yi`, `yo`, `yu`, `ou`
//   - Khử trùng lặp (`oai` từng xuất hiện 2 lần ở baseline gốc)
// ---------------------------------------------------------------------------
static const wchar_t* const s_nuclei[] = {
    // 3 nguyên âm — DÀI nhất, đặt trước
    L"i\u00eau",      // iêu
    L"y\u00eau",      // yêu
    L"\u01b0\u01a1u", // ươu
    L"u\u00f4i",      // uôi
    L"\u01b0\u01a1i", // ươi
    L"oai", L"oay",
    L"uya", L"uy\u00ea",            // uya, uyê
    L"oao", L"oeo", L"uyu", L"uye",
    L"ieu", L"yeu", L"uoi", L"uou", // ASCII Telex (chưa transform)

    // 2 nguyên âm
    L"ai", L"ao", L"au", L"ay",
    L"\u00e2u", L"\u00e2y",         // âu, ây
    L"eo", L"\u00eau",              // eo, êu
    L"ia", L"i\u00ea", L"ie", L"iu",
    L"oa", L"o\u0103", L"oe", L"oi", L"oo",  // oa, oă, oe, oi, oo
    L"\u00f4i",                      // ôi
    L"\u01a1i",                      // ơi
    L"ua", L"u\u00e2", L"u\u00ea", L"ui", L"u\u00f4", L"uy", L"uo", L"ue",
    L"\u01b0a", L"\u01b0i", L"\u01b0u", L"\u01b0\u01a1",  // ưa, ưi, ưu, ươ
    L"ya", L"y\u00ea", L"ye",        // ya, yê, ye

    // 1 nguyên âm
    L"a", L"\u0103", L"\u00e2",      // a, ă, â
    L"e", L"\u00ea",                  // e, ê
    L"i",
    L"o", L"\u00f4", L"\u01a1",      // o, ô, ơ
    L"u", L"\u01b0",                  // u, ư
    L"y"
};
static const int s_nucleiCount = sizeof(s_nuclei) / sizeof(s_nuclei[0]);

// ---------------------------------------------------------------------------
// PHẦN 2b: Bảng phụ âm cuối (finals)
// 8 phụ âm cuối hợp lệ trong âm tiết tiếng Việt. Sắp DÀI-TRƯỚC để
// `TryMatchFinal` thực hiện longest-prefix match đúng (vd. "ng" trước "n").
// "Không có phụ âm cuối" được biểu diễn bằng `TryMatchFinal` trả 0.
// ---------------------------------------------------------------------------
static const wchar_t* const s_finals[] = {
    L"ng", L"nh", L"ch",
    L"c",  L"m",  L"n", L"p", L"t"
};
static const int s_finalsCount = sizeof(s_finals) / sizeof(s_finals[0]);

// ---------------------------------------------------------------------------
// PHẦN 2c: Bảng vần phụ (tails)
// 4 vần phụ hợp lệ đứng sau nhân nguyên âm khi không có phụ âm cuối.
// Tất cả độ dài 1 nên thứ tự không ảnh hưởng longest-match.
// ---------------------------------------------------------------------------
static const wchar_t* const s_tails[] = {
    L"i", L"y", L"o", L"u"
};
static const int s_tailsCount = sizeof(s_tails) / sizeof(s_tails[0]);

// ---------------------------------------------------------------------------
// PHẦN 3: Bảng nguyên âm có dấu (single source of truth).
//
// Mỗi hàng = 1 nhóm nguyên âm: cột 0 = base (thuần hoặc đã có mũ/móc),
// cột 1..5 = 5 dạng có dấu thanh (huyền, sắc, hỏi, ngã, nặng).
//
// Tất cả lookup tone (GetToneMark, StripTone, LookupToneIndex) đều dùng
// bảng này — thay 3 switch khổng lồ thành 1 bảng nhỏ + linear search 12 hàng.
// 12 nhóm × 6 cột × 2 byte = 144 byte .rdata, gọn hơn ~600 byte switch.
// ---------------------------------------------------------------------------
static const wchar_t s_toneRows[12][6] = {
    // base       huyền    sắc      hỏi      ngã      nặng
    { L'a',       0x00E0,  0x00E1,  0x1EA3,  0x00E3,  0x1EA1 }, // a
    { 0x00E2,     0x1EA7,  0x1EA5,  0x1EA9,  0x1EAB,  0x1EAD }, // â
    { 0x0103,     0x1EB1,  0x1EAF,  0x1EB3,  0x1EB5,  0x1EB7 }, // ă
    { L'e',       0x00E8,  0x00E9,  0x1EBB,  0x1EBD,  0x1EB9 }, // e
    { 0x00EA,     0x1EC1,  0x1EBF,  0x1EC3,  0x1EC5,  0x1EC7 }, // ê
    { L'i',       0x00EC,  0x00ED,  0x1EC9,  0x0129,  0x1ECB }, // i
    { L'o',       0x00F2,  0x00F3,  0x1ECF,  0x00F5,  0x1ECD }, // o
    { 0x00F4,     0x1ED3,  0x1ED1,  0x1ED5,  0x1ED7,  0x1ED9 }, // ô
    { 0x01A1,     0x1EDD,  0x1EDB,  0x1EDF,  0x1EE1,  0x1EE3 }, // ơ
    { L'u',       0x00F9,  0x00FA,  0x1EE7,  0x0169,  0x1EE5 }, // u
    { 0x01B0,     0x1EEB,  0x1EE9,  0x1EED,  0x1EEF,  0x1EF1 }, // ư
    { L'y',       0x1EF3,  0x00FD,  0x1EF7,  0x1EF9,  0x1EF5 }  // y
};

// Tìm cell (row, col) trong s_toneRows chứa `ch` (đã chuẩn hoá lowercase).
// Trả false nếu không thuộc bảng nguyên âm.
static bool FindToneCell(wchar_t ch, int& row, int& col) {
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 6; j++) {
            if (s_toneRows[i][j] == ch) { row = i; col = j; return true; }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Helper file-scope: kiểm tra `s` (length `len`) khớp đúng một entry nào
// trong `table[0..tableCount)`. Dùng chung cho `IsValidInitial`/`IsValidNucleus`.
// ---------------------------------------------------------------------------
static bool ContainsExact(const wchar_t* const* table, int tableCount,
                          const wchar_t* s, int len) {
    if (!s || len <= 0) return false;
    for (int i = 0; i < tableCount; i++) {
        int elen = (int)CayStrLen(table[i]);
        if (elen == len && CayStrCmp(table[i], s) == 0) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// IsValidInitial - Kiểm tra phụ âm đầu hợp lệ
// ---------------------------------------------------------------------------
bool CayData::IsValidInitial(const wchar_t* s, int len) {
    return ContainsExact(s_initials, s_initialsCount, s, len);
}

// ---------------------------------------------------------------------------
// IsValidNucleus - Kiểm tra nguyên âm hợp lệ
// ---------------------------------------------------------------------------
bool CayData::IsValidNucleus(const wchar_t* s, int len) {
    return ContainsExact(s_nuclei, s_nucleiCount, s, len);
}

// ---------------------------------------------------------------------------
// GetToneIndex - Map phím Telex sang chỉ số dấu
// Map phím Telex modifier sang chỉ số 0–5; trả về -1 nếu không phải phím dấu.
// ---------------------------------------------------------------------------
int CayData::GetToneIndex(wchar_t key) {
    switch (key) {
    case L'z': case L'Z': return 0; // Xóa dấu  (flat)
    case L'f': case L'F': return 1; // Huyền
    case L's': case L'S': return 2; // Sắc
    case L'r': case L'R': return 3; // Hỏi
    case L'x': case L'X': return 4; // Ngã
    case L'j': case L'J': return 5; // Nặng
    default:               return -1;
    }
}

// ---------------------------------------------------------------------------
// GetToneMark - Lấy ký tự có dấu cho (nguyên âm cơ bản, chỉ số dấu).
// Tra qua `s_toneRows`: tìm hàng chứa `base`, trả ô cột `toneIndex`.
// Trả 0 nếu base không thuộc bảng hoặc toneIndex ngoài [0, 5].
// ---------------------------------------------------------------------------
wchar_t CayData::GetToneMark(wchar_t base, int toneIndex) {
    if (toneIndex < 0 || toneIndex > 5) return 0;
    int row, col;
    if (!FindToneCell(base, row, col)) return 0;
    return s_toneRows[row][toneIndex];
}

// ---------------------------------------------------------------------------
// GetHookRule - Lấy quy tắc dấu mũ
// ---------------------------------------------------------------------------
wchar_t CayData::GetHookRule(wchar_t c) {
    switch (c) {
        case L'a': return L'\u0103'; // ă
        case L'o': return L'\u01a1'; // ơ
        case L'u': return L'\u01b0'; // ư
        case L'A': return L'\u0102'; // Ă
        case L'O': return L'\u01A0'; // Ơ
        case L'U': return L'\u01AF'; // Ư
        case L'\u00e2': return L'\u0103'; // â -> ă
        case L'\u00C2': return L'\u0102'; // Â -> Ă
        default: return L'\0';
    }
}

// ---------------------------------------------------------------------------
// HasVietnameseMark (ký tự đơn) - Kiểm tra có dấu tiếng Việt
// `StripAccent` đã chain `StripTone` bên trong nên chỉ cần 1 lần gọi.
// ---------------------------------------------------------------------------
bool CayData::HasVietnameseMark(wchar_t ch) {
    if (ch < 0x00C0) return false;
    return StripAccent(ch) != ch;
}

// ---------------------------------------------------------------------------
// HasVietnameseMark (buffer) - Kiểm tra có dấu trong buffer
// ---------------------------------------------------------------------------
bool CayData::HasVietnameseMark(const wchar_t* buf, int len) {
    for (int i = 0; i < len; i++) {
        if (HasVietnameseMark(buf[i])) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// StripTone – Bỏ dấu thanh từ nguyên âm, giữ dấu mũ/dấu ngắn.
//
// Cài đặt: tra `s_toneRows` cho cả `ch` và `ToUpperViet(ch)`. Nếu match
// một ô (row, col) với `col >= 1` thì kết quả là cột 0 (base). Cột 0 hoặc
// không match → trả `ch` không đổi.
// ---------------------------------------------------------------------------
wchar_t CayData::StripTone(wchar_t ch) {
    int row, col;
    bool isUpper = (ch != ToLowerViet(ch));
    wchar_t lo = isUpper ? ToLowerViet(ch) : ch;
    if (!FindToneCell(lo, row, col)) return ch;
    if (col == 0) return ch; // không có dấu thanh
    wchar_t base = s_toneRows[row][0];
    return isUpper ? ToUpperViet(base) : base;
}

// ---------------------------------------------------------------------------
// StripAccent – Bỏ dấu mũ hoặc dấu ngắn, trả về nguyên âm ASCII thuần.
//
// Cài đặt: gọi `StripTone` trước để loại bỏ dấu thanh (cover các ký tự
// như `ầ`, `ằ`, `ề` ... đã có cả mũ + thanh), sau đó switch nhỏ chỉ
// chuyển 6 base có mũ/móc + đ về dạng ASCII tương ứng.
// ---------------------------------------------------------------------------
wchar_t CayData::StripAccent(wchar_t ch) {
    wchar_t c = StripTone(ch);
    switch (c) {
    case L'\u00E2': case L'\u0103': return L'a'; // â ă -> a
    case L'\u00C2': case L'\u0102': return L'A'; // Â Ă -> A
    case L'\u00EA':                 return L'e'; // ê   -> e
    case L'\u00CA':                 return L'E'; // Ê   -> E
    case L'\u00F4': case L'\u01A1': return L'o'; // ô ơ -> o
    case L'\u00D4': case L'\u01A0': return L'O'; // Ô Ơ -> O
    case L'\u01B0':                 return L'u'; // ư   -> u
    case L'\u01AF':                 return L'U'; // Ư   -> U
    case L'\u0111':                 return L'd'; // đ   -> d
    case L'\u0110':                 return L'D'; // Đ   -> D
    default: return c;
    }
}

// ---------------------------------------------------------------------------
// IsVowel – true cho bất kỳ nguyên âm tiếng Việt (thuần hoặc có dấu).
// `StripAccent` đã chain `StripTone` bên trong nên chỉ cần 1 lần gọi.
// ---------------------------------------------------------------------------
bool CayData::IsVowel(wchar_t ch) {
    wchar_t base = StripAccent(ch);
    switch (base) {
    case L'a': case L'A':
    case L'e': case L'E':
    case L'i': case L'I':
    case L'o': case L'O':
    case L'u': case L'U':
    case L'y': case L'Y':
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// ToLowerViet – Chuyển ký tự về chữ thường (Latin-1 + Vietnamese precomposed).
// Bao phủ:
//   - ASCII A..Z
//   - Latin-1 (À..Ý, trừ ×)
//   - U+01AF (Ư), U+0102..U+01A0 chẵn (Ă, Đ, Ĩ, Ũ, Ơ)
//   - U+1EA0..U+1EF8 chẵn (Ạ..Ỹ Latin Extended Additional)
// ---------------------------------------------------------------------------
wchar_t CayData::ToLowerViet(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return c + 32;
    if (c >= 0x00C0 && c <= 0x00DD && c != 0x00D7) return c + 0x20; // Latin-1
    if (c == 0x01AF) return 0x01B0; // Ư -> ư
    if (c >= 0x0102 && c <= 0x01A0 && (c % 2 == 0)) return c + 1;   // Ă, Đ, Ĩ, Ũ, Ơ
    if (c >= 0x1EA0 && c <= 0x1EF8 && (c % 2 == 0)) return c + 1;   // Ạ..Ỹ (Latin Extended Additional)
    return c;
}

// ---------------------------------------------------------------------------
// ToUpperViet – Chuyển ký tự về chữ hoa (đối xứng với ToLowerViet).
// ---------------------------------------------------------------------------
wchar_t CayData::ToUpperViet(wchar_t c) {
    if (c >= L'a' && c <= L'z') return c - 32;
    if (c >= 0x00E0 && c <= 0x00FD && c != 0x00F7) return c - 0x20;
    if (c == 0x01B0) return 0x01AF; // ư -> Ư
    if (c >= 0x0103 && c <= 0x01A1 && (c % 2 != 0)) return c - 1;
    if (c >= 0x1EA1 && c <= 0x1EF9 && (c % 2 != 0)) return c - 1;
    return c;
}

// ---------------------------------------------------------------------------
// LookupToneIndex – tra ngược chỉ số dấu thanh của ký tự nguyên âm thường.
//
// Tra `s_toneRows`: nếu match ô (row, col) thì trả `col` (0 = không có dấu).
// Trả 0 nếu `c_lower` không thuộc bảng (caller đã chuẩn hoá lowercase trước).
//
// Static helper file-scope — chi tiết implementation của `DecomposeChar`.
// ---------------------------------------------------------------------------
static int LookupToneIndex(wchar_t c_lower) {
    int row, col;
    if (!FindToneCell(c_lower, row, col)) return 0;
    return col;
}

// ---------------------------------------------------------------------------
// DecomposeChar – tách `c` thành (base, toneIndex, isUpper) trong O(1).
// ---------------------------------------------------------------------------
DecomposedChar CayData::DecomposeChar(wchar_t c) {
    DecomposedChar r;
    wchar_t lo   = ToLowerViet(c);
    r.isUpper    = (c != lo);
    r.toneIndex  = LookupToneIndex(lo);
    r.base       = StripTone(lo);
    return r;
}

// ---------------------------------------------------------------------------
// ComposeChar – ghép (base, toneIndex, isUpper) trở lại ký tự có dấu.
//
// Pre: `base` là nguyên âm cơ bản (thuần hoặc có mũ/móc), viết thường.
//      `toneIndex` 0..5 — ngoài khoảng coi như 0 thông qua `GetToneMark`.
// Post: round-trip với `DecomposeChar` cho mọi ký tự CayData hỗ trợ.
// ---------------------------------------------------------------------------
wchar_t CayData::ComposeChar(wchar_t base, int toneIndex, bool isUpper) {
    wchar_t c;
    if (toneIndex == 0) {
        c = base;
    } else {
        c = GetToneMark(base, toneIndex);
        if (c == 0) c = base; // Fallback: base không có mapping tone → giữ nguyên.
    }
    return isUpper ? ToUpperViet(c) : c;
}

// ---------------------------------------------------------------------------
// TryMatch* — Longest-prefix match trên các bảng âm tiết.
//
// Pattern chung: duyệt bảng (đã sắp DÀI-TRƯỚC), với mỗi entry kiểm tra
//   1. `entryLen <= len`               (đủ chỗ trong buffer)
//   2. so từng `wchar_t` của entry với `s[0..entryLen)`
// Trả `entryLen` ngay khi có entry khớp; trả `0` nếu không entry nào khớp.
//
// Không dùng CRT, không STL. Bảng `s_initials` và `s_nuclei` được tham chiếu
// trực tiếp (single source of truth — không clone).
// ---------------------------------------------------------------------------
static int TryMatchInTable(const wchar_t* const* table, int tableCount,
                           const wchar_t* s, int len) {
    if (!s || len <= 0) return 0;
    for (int i = 0; i < tableCount; i++) {
        const wchar_t* entry = table[i];
        int entryLen = (int)CayStrLen(entry);
        if (entryLen <= 0 || entryLen > len) continue;
        bool match = true;
        for (int k = 0; k < entryLen; k++) {
            if (entry[k] != s[k]) { match = false; break; }
        }
        if (match) return entryLen;
    }
    return 0;
}

int CayData::TryMatchInitial(const wchar_t* s, int len) {
    return TryMatchInTable(s_initials, s_initialsCount, s, len);
}

int CayData::TryMatchNucleus(const wchar_t* s, int len) {
    return TryMatchInTable(s_nuclei, s_nucleiCount, s, len);
}

int CayData::TryMatchFinal(const wchar_t* s, int len) {
    return TryMatchInTable(s_finals, s_finalsCount, s, len);
}

int CayData::TryMatchTail(const wchar_t* s, int len) {
    return TryMatchInTable(s_tails, s_tailsCount, s, len);
}

} // namespace Cay

