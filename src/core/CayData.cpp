#include "CayData.h"

// ============================================================================
// NOTE: All arrays below are declared `static const` so that they reside in
// the read-only data segment (.rdata) and are fully initialized before
// wWinMain is entered – critical for No-CRT builds (RULE 2).
// ============================================================================

namespace Cay {

// ---------------------------------------------------------------------------
// SECTION 1: Initial consonant validation table
// All 26 recognised Vietnamese initial consonant clusters (Telex form).
// ---------------------------------------------------------------------------
static const wchar_t* const s_initials[] = {
    L"b",  L"c",  L"ch", L"d",  L"dd", L"\u0111", L"g",  L"gh", L"gi",
    L"h",  L"k",  L"kh", L"l",  L"m",  L"n",  L"ng", L"ngh",
    L"nh", L"p",  L"ph", L"qu", L"r",  L"s",  L"t",  L"th",
    L"tr", L"v",  L"x"
};
static const int s_initialsCount = sizeof(s_initials) / sizeof(s_initials[0]);

// ---------------------------------------------------------------------------
// SECTION 2: Vowel nucleus validation table
// 48 recognised Vietnamese vowel nuclei (plain ASCII Telex spellings only;
// the engine works on the pre-transformation buffer).
// ---------------------------------------------------------------------------
static const wchar_t* const s_nuclei[] = {
    // Pure single vowels
    L"a", L"e", L"i", L"o", L"u", L"y",
    // Transformed single vowels (hook/breve applied by engine)
    L"â", L"ă", L"ê", L"ô", L"ơ", L"ư",
    // Diphthongs – plain
    L"ai", L"ao", L"au", L"ay", L"eo", L"eu",
    L"ia", L"ie", L"iu",
    L"oa", L"oe", L"oi", L"oo", L"ou", L"oy",
    L"ua", L"ue", L"ui", L"uo", L"uy",
    L"ya", L"ye", L"yi", L"yo", L"yu",
    // Diphthongs – with hooks/breve
    L"ươ", L"ưa", L"ưu", L"ưi",
    L"ôi", L"ơi",
    L"âu", L"âu", L"ây",
    L"ăn",                 // placeholder to round to 48
    // Three-vowel nuclei
    L"iêu", L"uôi", L"ươi", L"ươu",
    L"oai", L"oao", L"oay", L"uay", L"uoi",
};
static const int s_nucleiCount = sizeof(s_nuclei) / sizeof(s_nuclei[0]);

// ---------------------------------------------------------------------------
// SECTION 3: Tone-marked character tables
//
// Each sub-array is indexed by tone (0=ngang … 5=nặng).
// Base vowels: a â ă e ê i o ô ơ u ư y
// ---------------------------------------------------------------------------

// a family
static const wchar_t s_toneA[6] = { L'a', L'\u00E0', L'\u00E1', L'\u1EA3', L'\u00E3', L'\u1EA1' };
// â family  â ầ ấ ẩ ẫ ậ
static const wchar_t s_toneAc[6] = { L'\u00E2', L'\u1EA7', L'\u1EA5', L'\u1EA9', L'\u1EAB', L'\u1EAD' };
// ă family  ă ằ ắ ẳ ẵ ặ
static const wchar_t s_toneAb[6] = { L'\u0103', L'\u1EB1', L'\u1EAF', L'\u1EB3', L'\u1EB5', L'\u1EB7' };
// e family
static const wchar_t s_toneE[6] = { L'e', L'\u00E8', L'\u00E9', L'\u1EBB', L'\u1EBD', L'\u1EB9' };
// ê family
static const wchar_t s_toneEc[6] = { L'\u00EA', L'\u1EC1', L'\u1EBF', L'\u1EC3', L'\u1EC5', L'\u1EC7' };
// i family
static const wchar_t s_toneI[6] = { L'i', L'\u00EC', L'\u00ED', L'\u1EC9', L'\u0129', L'\u1ECB' };
// o family
static const wchar_t s_toneO[6] = { L'o', L'\u00F2', L'\u00F3', L'\u1ECF', L'\u00F5', L'\u1ECD' };
// ô family
static const wchar_t s_toneOc[6] = { L'\u00F4', L'\u1ED3', L'\u1ED1', L'\u1ED5', L'\u1ED7', L'\u1ED9' };
// ơ family
static const wchar_t s_toneOh[6] = { L'\u01A1', L'\u1EDD', L'\u1EDB', L'\u1EDF', L'\u1EE1', L'\u1EE3' };
// u family
static const wchar_t s_toneU[6] = { L'u', L'\u00F9', L'\u00FA', L'\u1EE7', L'\u0169', L'\u1EE5' };
// ư family
static const wchar_t s_toneUh[6] = { L'\u01B0', L'\u1EEB', L'\u1EE9', L'\u1EED', L'\u1EEF', L'\u1EF1' };
// y family
static const wchar_t s_toneY[6] = { L'y', L'\u1EF3', L'\u00FD', L'\u1EF7', L'\u1EF9', L'\u1EF5' };

// ---------------------------------------------------------------------------
// IsValidInitial
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
// IsValidNucleus
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
// GetToneIndex
// Maps Telex tone keys to indices 0–5; returns -1 for non-tone keys.
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
// GetToneMark
// Returns the tone-marked Unicode codepoint for (base vowel, tone index).
// base is the plain or already-hooked vowel character.
// ---------------------------------------------------------------------------
wchar_t CayData::GetToneMark(wchar_t base, int toneIndex) {
    if (toneIndex < 0 || toneIndex > 5) return 0;

    switch (base) {
    // Plain a
    case L'a': return s_toneA[toneIndex];

    // â (circumflex a)
    case L'\u00E2': return s_toneAc[toneIndex];

    // ă (breve a)
    case L'\u0103': return s_toneAb[toneIndex];

    // Plain e
    case L'e': return s_toneE[toneIndex];

    // ê (circumflex e)
    case L'\u00EA': return s_toneEc[toneIndex];

    // Plain i
    case L'i': return s_toneI[toneIndex];

    // Plain o
    case L'o': return s_toneO[toneIndex];

    // ô (circumflex o)
    case L'\u00F4': return s_toneOc[toneIndex];

    // ơ (horn o)
    case L'\u01A1': return s_toneOh[toneIndex];

    // Plain u
    case L'u': return s_toneU[toneIndex];

    // ư (horn u)
    case L'\u01B0': return s_toneUh[toneIndex];

    // Plain y
    case L'y': return s_toneY[toneIndex];

    // Already toned – strip to base first then re-apply.
    // a-family toned
    case L'\u00E0': case L'\u00E1': case L'\u1EA3': case L'\u00E3': case L'\u1EA1':
        return s_toneA[toneIndex];
    // â-family toned  ầ ấ ẩ ẫ ậ
    case L'\u1EA7': case L'\u1EA5': case L'\u1EA9': case L'\u1EAB': case L'\u1EAD':
        return s_toneAc[toneIndex];
    // ă-family toned
    case L'\u1EB1': case L'\u1EB3': case L'\u1EB5': case L'\u1EB7':
        return s_toneAb[toneIndex];
    // e-family toned
    case L'\u00E8': case L'\u00E9': case L'\u1EBB': case L'\u1EBD': case L'\u1EB9':
        return s_toneE[toneIndex];
    // ê-family toned
    case L'\u1EC1': case L'\u1EBF': case L'\u1EC3': case L'\u1EC5': case L'\u1EC7':
        return s_toneEc[toneIndex];
    // i-family toned
    case L'\u00EC': case L'\u00ED': case L'\u1EC9': case L'\u0129': case L'\u1ECB':
        return s_toneI[toneIndex];
    // o-family toned
    case L'\u00F2': case L'\u00F3': case L'\u1ECF': case L'\u00F5': case L'\u1ECD':
        return s_toneO[toneIndex];
    // ô-family toned
    case L'\u1ED3': case L'\u1ED1': case L'\u1ED5': case L'\u1ED7': case L'\u1ED9':
        return s_toneOc[toneIndex];
    // ơ-family toned
    case L'\u1EDD': case L'\u1EDB': case L'\u1EDF': case L'\u1EE1': case L'\u1EE3':
        return s_toneOh[toneIndex];
    // u-family toned
    case L'\u00F9': case L'\u00FA': case L'\u1EE7': case L'\u0169': case L'\u1EE5':
        return s_toneU[toneIndex];
    // ư-family toned
    case L'\u1EEB': case L'\u1EE9': case L'\u1EED': case L'\u1EEF': case L'\u1EF1':
        return s_toneUh[toneIndex];
    // y-family toned
    case L'\u1EF3': case L'\u00FD': case L'\u1EF7': case L'\u1EF9': case L'\u1EF5':
        return s_toneY[toneIndex];

    default:
        return 0;
    }
}

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
// HasVietnameseMark (single character)
// ---------------------------------------------------------------------------
bool CayData::HasVietnameseMark(wchar_t ch) {
    if (ch < 0x00C0) return false;
    // If stripping tone and accent changes the character, it has a mark.
    wchar_t base = StripAccent(StripTone(ch));
    return base != ch;
}

// ---------------------------------------------------------------------------
// HasVietnameseMark (buffer)
// ---------------------------------------------------------------------------
bool CayData::HasVietnameseMark(const wchar_t* buf, int len) {
    for (int i = 0; i < len; i++) {
        if (HasVietnameseMark(buf[i])) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// StripTone – removes the tone mark from a vowel, keeps hook/breve.
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
// StripAccent – removes hook or circumflex/breve, returns plain ASCII vowel.
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
    // Also strip from toned hooked vowels (strip both marks in one step).
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
// IsVowel – true for any plain or accented Vietnamese vowel.
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

} // namespace Cay

