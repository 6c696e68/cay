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
// PHẦN 3: Bảng ký tự có dấu
//
// Mỗi sub-array được index theo dấu (0=ngang … 5=nặng).
// Nguyên âm cơ bản: a â ă e ê i o ô ơ u ư y
// ---------------------------------------------------------------------------

// Nhóm a
static const wchar_t s_toneA[6] = { L'a', L'\u00E0', L'\u00E1', L'\u1EA3', L'\u00E3', L'\u1EA1' };
// Nhóm â  â ầ ấ ẩ ẫ ậ
static const wchar_t s_toneAc[6] = { L'\u00E2', L'\u1EA7', L'\u1EA5', L'\u1EA9', L'\u1EAB', L'\u1EAD' };
// Nhóm ă  ă ằ ắ ẳ ẵ ặ
static const wchar_t s_toneAb[6] = { L'\u0103', L'\u1EB1', L'\u1EAF', L'\u1EB3', L'\u1EB5', L'\u1EB7' };
// Nhóm e
static const wchar_t s_toneE[6] = { L'e', L'\u00E8', L'\u00E9', L'\u1EBB', L'\u1EBD', L'\u1EB9' };
// Nhóm ê
static const wchar_t s_toneEc[6] = { L'\u00EA', L'\u1EC1', L'\u1EBF', L'\u1EC3', L'\u1EC5', L'\u1EC7' };
// Nhóm i
static const wchar_t s_toneI[6] = { L'i', L'\u00EC', L'\u00ED', L'\u1EC9', L'\u0129', L'\u1ECB' };
// Nhóm o
static const wchar_t s_toneO[6] = { L'o', L'\u00F2', L'\u00F3', L'\u1ECF', L'\u00F5', L'\u1ECD' };
// Nhóm ô
static const wchar_t s_toneOc[6] = { L'\u00F4', L'\u1ED3', L'\u1ED1', L'\u1ED5', L'\u1ED7', L'\u1ED9' };
// Nhóm ơ
static const wchar_t s_toneOh[6] = { L'\u01A1', L'\u1EDD', L'\u1EDB', L'\u1EDF', L'\u1EE1', L'\u1EE3' };
// Nhóm u
static const wchar_t s_toneU[6] = { L'u', L'\u00F9', L'\u00FA', L'\u1EE7', L'\u0169', L'\u1EE5' };
// Nhóm ư
static const wchar_t s_toneUh[6] = { L'\u01B0', L'\u1EEB', L'\u1EE9', L'\u1EED', L'\u1EEF', L'\u1EF1' };
// Nhóm y
static const wchar_t s_toneY[6] = { L'y', L'\u1EF3', L'\u00FD', L'\u1EF7', L'\u1EF9', L'\u1EF5' };

// ---------------------------------------------------------------------------
// IsValidInitial - Kiểm tra phụ âm đầu hợp lệ
// ---------------------------------------------------------------------------
bool CayData::IsValidInitial(const wchar_t* s, int len) {
    if (!s || len <= 0) return false;
    for (int i = 0; i < s_initialsCount; i++) {
        if ((int)CayStrLen(s_initials[i]) == len &&
            CayStrCmp(s_initials[i], s) == 0) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// IsValidNucleus - Kiểm tra nguyên âm hợp lệ
// ---------------------------------------------------------------------------
bool CayData::IsValidNucleus(const wchar_t* s, int len) {
    if (!s || len <= 0) return false;
    for (int i = 0; i < s_nucleiCount; i++) {
        int nlen = (int)CayStrLen(s_nuclei[i]);
        if (nlen == len && CayStrCmp(s_nuclei[i], s) == 0) {
            return true;
        }
    }
    return false;
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
// GetToneMark - Lấy ký tự có dấu
// Trả về codepoint Unicode có dấu cho (nguyên âm cơ bản, chỉ số dấu).
// base là nguyên âm thuần hoặc đã có dấu mũ.
// ---------------------------------------------------------------------------
wchar_t CayData::GetToneMark(wchar_t base, int toneIndex) {
    if (toneIndex < 0 || toneIndex > 5) return 0;

    switch (base) {
    // a thuần
    case L'a': return s_toneA[toneIndex];

    // â (dấu mũ trên a)
    case L'\u00E2': return s_toneAc[toneIndex];

    // ă (dấu ngắn trên a)
    case L'\u0103': return s_toneAb[toneIndex];

    // e thuần
    case L'e': return s_toneE[toneIndex];

    // ê (dấu mũ trên e)
    case L'\u00EA': return s_toneEc[toneIndex];

    // i thuần
    case L'i': return s_toneI[toneIndex];

    // o thuần
    case L'o': return s_toneO[toneIndex];

    // ô (dấu mũ trên o)
    case L'\u00F4': return s_toneOc[toneIndex];

    // ơ (dấu móc trên o)
    case L'\u01A1': return s_toneOh[toneIndex];

    // u thuần
    case L'u': return s_toneU[toneIndex];

    // ư (dấu móc trên u)
    case L'\u01B0': return s_toneUh[toneIndex];

    // y thuần
    case L'y': return s_toneY[toneIndex];

    // Đã có dấu – bỏ về cơ bản rồi áp dụng lại.
    // Nhóm a đã có dấu
    case L'\u00E0': case L'\u00E1': case L'\u1EA3': case L'\u00E3': case L'\u1EA1':
        return s_toneA[toneIndex];
    // Nhóm â đã có dấu  ầ ấ ẩ ẫ ậ
    case L'\u1EA7': case L'\u1EA5': case L'\u1EA9': case L'\u1EAB': case L'\u1EAD':
        return s_toneAc[toneIndex];
    // Nhóm ă đã có dấu
    case L'\u1EB1': case L'\u1EB3': case L'\u1EB5': case L'\u1EB7':
        return s_toneAb[toneIndex];
    // Nhóm e đã có dấu
    case L'\u00E8': case L'\u00E9': case L'\u1EBB': case L'\u1EBD': case L'\u1EB9':
        return s_toneE[toneIndex];
    // Nhóm ê đã có dấu
    case L'\u1EC1': case L'\u1EBF': case L'\u1EC3': case L'\u1EC5': case L'\u1EC7':
        return s_toneEc[toneIndex];
    // Nhóm i đã có dấu
    case L'\u00EC': case L'\u00ED': case L'\u1EC9': case L'\u0129': case L'\u1ECB':
        return s_toneI[toneIndex];
    // Nhóm o đã có dấu
    case L'\u00F2': case L'\u00F3': case L'\u1ECF': case L'\u00F5': case L'\u1ECD':
        return s_toneO[toneIndex];
    // Nhóm ô đã có dấu
    case L'\u1ED3': case L'\u1ED1': case L'\u1ED5': case L'\u1ED7': case L'\u1ED9':
        return s_toneOc[toneIndex];
    // Nhóm ơ đã có dấu
    case L'\u1EDD': case L'\u1EDB': case L'\u1EDF': case L'\u1EE1': case L'\u1EE3':
        return s_toneOh[toneIndex];
    // Nhóm u đã có dấu
    case L'\u00F9': case L'\u00FA': case L'\u1EE7': case L'\u0169': case L'\u1EE5':
        return s_toneU[toneIndex];
    // Nhóm ư đã có dấu
    case L'\u1EEB': case L'\u1EE9': case L'\u1EED': case L'\u1EEF': case L'\u1EF1':
        return s_toneUh[toneIndex];
    // Nhóm y đã có dấu
    case L'\u1EF3': case L'\u00FD': case L'\u1EF7': case L'\u1EF9': case L'\u1EF5':
        return s_toneY[toneIndex];

    default:
        return 0;
    }
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
// ---------------------------------------------------------------------------
bool CayData::HasVietnameseMark(wchar_t ch) {
    if (ch < 0x00C0) return false;
    // Nếu bỏ dấu thanh và dấu mũ làm thay đổi ký tự, có nghĩa là có dấu.
    wchar_t base = StripAccent(StripTone(ch));
    return base != ch;
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
// ---------------------------------------------------------------------------
wchar_t CayData::StripTone(wchar_t ch) {
    switch (ch) {
        case L'\u00E0': case L'\u00E1': case L'\u1EA3': case L'\u00E3': case L'\u1EA1': return L'a';
        case L'\u00C0': case L'\u00C1': case L'\u1EA2': case L'\u00C3': case L'\u1EA0': return L'A';
        case L'\u1EA7': case L'\u1EA5': case L'\u1EA9': case L'\u1EAB': case L'\u1EAD': return L'\u00E2';
        case L'\u1EA6': case L'\u1EA4': case L'\u1EA8': case L'\u1EAA': case L'\u1EAC': return L'\u00C2';
        case L'\u1EB1': case L'\u1EAF': case L'\u1EB3': case L'\u1EB5': case L'\u1EB7': return L'\u0103';
        case L'\u1EB0': case L'\u1EAE': case L'\u1EB2': case L'\u1EB4': case L'\u1EB6': return L'\u0102';
        case L'\u00E8': case L'\u00E9': case L'\u1EBB': case L'\u1EBD': case L'\u1EB9': return L'e';
        case L'\u00C8': case L'\u00C9': case L'\u1EBA': case L'\u1EBC': case L'\u1EB8': return L'E';
        case L'\u1EC1': case L'\u1EBF': case L'\u1EC3': case L'\u1EC5': case L'\u1EC7': return L'\u00EA';
        case L'\u1EC0': case L'\u1EBE': case L'\u1EC2': case L'\u1EC4': case L'\u1EC6': return L'\u00CA';
        case L'\u00EC': case L'\u00ED': case L'\u1EC9': case L'\u0129': case L'\u1ECB': return L'i';
        case L'\u00CC': case L'\u00CD': case L'\u1EC8': case L'\u0128': case L'\u1ECA': return L'I';
        case L'\u00F2': case L'\u00F3': case L'\u1ECF': case L'\u00F5': case L'\u1ECD': return L'o';
        case L'\u00D2': case L'\u00D3': case L'\u1ECE': case L'\u00D5': case L'\u1ECC': return L'O';
        case L'\u1ED3': case L'\u1ED1': case L'\u1ED5': case L'\u1ED7': case L'\u1ED9': return L'\u00F4';
        case L'\u1ED2': case L'\u1ED0': case L'\u1ED4': case L'\u1ED6': case L'\u1ED8': return L'\u00D4';
        case L'\u1EDD': case L'\u1EDB': case L'\u1EDF': case L'\u1EE1': case L'\u1EE3': return L'\u01A1';
        case L'\u1EDC': case L'\u1EDA': case L'\u1EDE': case L'\u1EE0': case L'\u1EE2': return L'\u01A0';
        case L'\u00F9': case L'\u00FA': case L'\u1EE7': case L'\u0169': case L'\u1EE5': return L'u';
        case L'\u00D9': case L'\u00DA': case L'\u1EE6': case L'\u0168': case L'\u1EE4': return L'U';
        case L'\u1EEB': case L'\u1EE9': case L'\u1EED': case L'\u1EEF': case L'\u1EF1': return L'\u01B0';
        case L'\u1EEA': case L'\u1EE8': case L'\u1EEC': case L'\u1EEE': case L'\u1EF0': return L'\u01AF';
        case L'\u1EF3': case L'\u00FD': case L'\u1EF7': case L'\u1EF9': case L'\u1EF5': return L'y';
        case L'\u1EF2': case L'\u00DD': case L'\u1EF6': case L'\u1EF8': case L'\u1EF4': return L'Y';
        default: return ch;
    }
}

// ---------------------------------------------------------------------------
// StripAccent – Bỏ dấu mũ hoặc dấu ngắn, trả về nguyên âm ASCII thuần.
// ---------------------------------------------------------------------------
wchar_t CayData::StripAccent(wchar_t ch) {
    switch (ch) {
    case L'\u00E2': case L'\u0103': return L'a'; // â ă -> a
    case L'\u00C2': case L'\u0102': return L'A'; // Â Ă -> A
    case L'\u00EA':                 return L'e'; // ê   -> e
    case L'\u00CA':                 return L'E'; // Ê   -> E
    case L'\u00F4':                 return L'o'; // ô   -> o
    case L'\u00D4':                 return L'O'; // Ô   -> O
    case L'\u01A1':                 return L'o'; // ơ   -> o
    case L'\u01A0':                 return L'O'; // Ơ   -> O
    case L'\u01B0':                 return L'u'; // ư   -> u
    case L'\u01AF':                 return L'U'; // Ư   -> U
    case L'\u0111':                 return L'd'; // đ   -> d
    case L'\u0110':                 return L'D'; // Đ   -> D
    // Cũng bỏ từ nguyên âm có dấu mũ đã có dấu (bỏ cả 2 dấu trong 1 bước).
    case L'\u1EA7': case L'\u1EA5': case L'\u1EAB': case L'\u1EAD': case L'\u1EAF': return L'a';
    case L'\u1EA6': case L'\u1EA4': case L'\u1EAA': case L'\u1EAC': case L'\u1EAE': return L'A';
    case L'\u1EB1': case L'\u1EB3': case L'\u1EB5': case L'\u1EB7': return L'a';
    case L'\u1EB0': case L'\u1EB2': case L'\u1EB4': case L'\u1EB6': return L'A';
    case L'\u1EC1': case L'\u1EBF': case L'\u1EC3': case L'\u1EC5': case L'\u1EC7': return L'e';
    case L'\u1EC0': case L'\u1EBE': case L'\u1EC2': case L'\u1EC4': case L'\u1EC6': return L'E';
    case L'\u1ED3': case L'\u1ED1': case L'\u1ED5': case L'\u1ED7': case L'\u1ED9': return L'o';
    case L'\u1ED2': case L'\u1ED0': case L'\u1ED4': case L'\u1ED6': case L'\u1ED8': return L'O';
    case L'\u1EDD': case L'\u1EDB': case L'\u1EDF': case L'\u1EE1': case L'\u1EE3': return L'o';
    case L'\u1EDC': case L'\u1EDA': case L'\u1EDE': case L'\u1EE0': case L'\u1EE2': return L'O';
    case L'\u1EEB': case L'\u1EE9': case L'\u1EED': case L'\u1EEF': case L'\u1EF1': return L'u';
    case L'\u1EEA': case L'\u1EE8': case L'\u1EEC': case L'\u1EEE': case L'\u1EF0': return L'U';
    default: return ch;
    }
}

// ---------------------------------------------------------------------------
// IsVowel – true cho bất kỳ nguyên âm tiếng Việt (thuần hoặc có dấu).
// ---------------------------------------------------------------------------
bool CayData::IsVowel(wchar_t ch) {
    wchar_t base = StripAccent(StripTone(ch));
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
// Nhận `c_lower` đã được `ToLowerViet` chuẩn hoá. Trả về:
//   1 = huyền, 2 = sắc, 3 = hỏi, 4 = ngã, 5 = nặng, 0 = không có dấu thanh
//   (bao gồm cả nguyên âm thuần và nguyên âm có dấu mũ/móc không kèm thanh).
//
// Switch trải đủ 12 nguyên âm × 5 tone = 60 case (compiler MSVC/clang sẽ
// tối ưu thành jump table). Không vòng for, không CRT — O(1) thuần tuý.
//
// Đây là static helper file-scope (không phải member của CayData) — nó là
// chi tiết implementation của `DecomposeChar` và không cần expose ra ngoài.
// ---------------------------------------------------------------------------
static int LookupToneIndex(wchar_t c_lower) {
    switch (c_lower) {
        // Tone 1 — huyền (à, ầ, ằ, è, ề, ì, ò, ồ, ờ, ù, ừ, ỳ)
        case L'\u00E0': case L'\u1EA7': case L'\u1EB1':
        case L'\u00E8': case L'\u1EC1': case L'\u00EC':
        case L'\u00F2': case L'\u1ED3': case L'\u1EDD':
        case L'\u00F9': case L'\u1EEB': case L'\u1EF3':
            return 1;

        // Tone 2 — sắc (á, ấ, ắ, é, ế, í, ó, ố, ớ, ú, ứ, ý)
        case L'\u00E1': case L'\u1EA5': case L'\u1EAF':
        case L'\u00E9': case L'\u1EBF': case L'\u00ED':
        case L'\u00F3': case L'\u1ED1': case L'\u1EDB':
        case L'\u00FA': case L'\u1EE9': case L'\u00FD':
            return 2;

        // Tone 3 — hỏi (ả, ẩ, ẳ, ẻ, ể, ỉ, ỏ, ổ, ở, ủ, ử, ỷ)
        case L'\u1EA3': case L'\u1EA9': case L'\u1EB3':
        case L'\u1EBB': case L'\u1EC3': case L'\u1EC9':
        case L'\u1ECF': case L'\u1ED5': case L'\u1EDF':
        case L'\u1EE7': case L'\u1EED': case L'\u1EF7':
            return 3;

        // Tone 4 — ngã (ã, ẫ, ẵ, ẽ, ễ, ĩ, õ, ỗ, ỡ, ũ, ữ, ỹ)
        case L'\u00E3': case L'\u1EAB': case L'\u1EB5':
        case L'\u1EBD': case L'\u1EC5': case L'\u0129':
        case L'\u00F5': case L'\u1ED7': case L'\u1EE1':
        case L'\u0169': case L'\u1EEF': case L'\u1EF9':
            return 4;

        // Tone 5 — nặng (ạ, ậ, ặ, ẹ, ệ, ị, ọ, ộ, ợ, ụ, ự, ỵ)
        case L'\u1EA1': case L'\u1EAD': case L'\u1EB7':
        case L'\u1EB9': case L'\u1EC7': case L'\u1ECB':
        case L'\u1ECD': case L'\u1ED9': case L'\u1EE3':
        case L'\u1EE5': case L'\u1EF1': case L'\u1EF5':
            return 5;

        default:
            return 0;
    }
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

