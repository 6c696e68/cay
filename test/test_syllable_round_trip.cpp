// =====================================================================
// test/test_syllable_round_trip.cpp
//
// Property 13 — Round-trip IsCompleteSyllable với pretty-printer
// Property 16 — IsValidNucleus đúng trên corpus
//
// Spec: .kiro/specs/engine-core-refactor/tasks.md task 4.4..4.5
//
// P13 Validates: Requirements 17.2, 17.3, 17.4
// P16 Validates: Requirements 3.4, 3.5, 3.6
// =====================================================================

#include "doctest.h"

#include "CayData.h"
#include "CayEngine.h"
#include "corpus.h"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

// ---------------------------------------------------------------------
// Pretty-printer: ghép 4 thành phần (initial + nucleus + final + tail)
// thành 1 chuỗi `wstring`. Mỗi pointer có thể trỏ tới chuỗi rỗng.
//
// Caller dùng làm helper test cho Property 13 (Requirement 17.1).
// ---------------------------------------------------------------------
std::wstring prettyPrintSyllable(const wchar_t* initial,
                                 const wchar_t* nucleus,
                                 const wchar_t* finalPart,
                                 const wchar_t* tail) {
    std::wstring out;
    if (initial)   out.append(initial);
    if (nucleus)   out.append(nucleus);
    if (finalPart) out.append(finalPart);
    if (tail)      out.append(tail);
    return out;
}

// ---------------------------------------------------------------------
// Bảng dùng cho enumeration (ở dạng `IsCompleteSyllable` mong đợi:
// đã `ToLowerViet(StripTone(...))` — không có dấu thanh, viết thường).
// Mỗi entry là literal đã có sẵn trong `s_initials`/`s_nuclei`/
// `s_finals`/`s_tails` của CayData (single source of truth).
//
// Lưu ý:
//   - Bỏ initial "gi" để tránh special-case rollback ('gi' + non-vowel
//     thì 'i' tách thành nucleus) — chuyện này không phải bug, chỉ là
//     làm test enumeration đỡ nhiễu.
//   - Bỏ "ngh"/"gh" vì chúng đặt ràng buộc nucleus phải bắt đầu bằng
//     i/e/ê/y; ràng buộc đó nằm trong bypass logic, KHÔNG nằm trong
//     `IsCompleteSyllable`. Vẫn đưa vào để xác minh structural validator.
//   - "qu" là initial Vietnam hợp lệ duy nhất bắt đầu bằng 'q' — q đơn
//     KHÔNG có trong s_initials.
// ---------------------------------------------------------------------
const wchar_t* const kInitials[] = {
    L"",     L"b",   L"c",   L"ch",  L"d",   L"\u0111", L"g",  L"gh",
    L"h",    L"k",   L"kh",  L"l",   L"m",   L"n",       L"ng", L"ngh",
    L"nh",   L"p",   L"ph",  L"qu",  L"r",   L"s",       L"t",  L"th",
    L"tr",   L"v",   L"x"
};

// 2 nguyên âm + 3 nguyên âm + 1 nguyên âm — coverage rộng cho longest-match
// trong `TryMatchNucleus`.
const wchar_t* const kNuclei[] = {
    // 1 nguyên âm
    L"a",                L"\u0103",            L"\u00e2",
    L"e",                L"\u00ea",
    L"i",
    L"o",                L"\u00f4",            L"\u01a1",
    L"u",                L"\u01b0",
    L"y",
    // 2 nguyên âm
    L"oa",               L"oe",                L"u\u00ea",         L"uy",
    L"i\u00ea",          L"y\u00ea",
    L"\u00e2u",          L"\u00eau",
    // 3 nguyên âm
    L"i\u00eau",         L"y\u00eau",
    L"\u01b0\u01a1u",
};

// Phần tử "" biểu diễn "không có phụ âm cuối".
const wchar_t* const kFinals[] = {
    L"",  L"c",  L"ch", L"m",  L"n",  L"ng", L"nh", L"p", L"t"
};

// ---------------------------------------------------------------------
// Re-tokenization filter: parser dùng longest-prefix-match nên cặp
// (initial="g", nucleus[0]='i') sẽ bị reparse thành (initial="gi", nucleus
// rút gọn 1 ký tự) khi `gi`-rollback không kích hoạt — tức khi sau "gi"
// vẫn còn nguyên âm. Vd: pretty-print(g, iê, c) = "giêc" → parser thấy
// initial="gi", nucleus="ê", final="c" → fail pairing ê+c.
//
// Để bảo toàn print → parse round-trip (Requirement 17.2), test bỏ qua
// các bộ ambiguous này: pretty-printer KHÔNG có cách nào ép parser giữ
// nguyên 'g' khi tiếp theo là 'i' + nguyên âm khác.
// ---------------------------------------------------------------------
bool isAmbiguousInitialNucleus(const wchar_t* initial,
                               const wchar_t* nucleus) {
    return initial && initial[0] == L'g' && initial[1] == L'\0'
        && nucleus && nucleus[0] == L'i';
}

// ---------------------------------------------------------------------
// Pairing-rule predicate (mirror logic in IsCompleteSyllable):
//   - nh/ch CHỈ đi với a/i/ê/y/oa/uy/uê.
//   - ng/c KHÔNG đi với i/ê/y.
// Trả `true` nếu cặp `(nucleus, final)` hợp lệ theo quy tắc.
// ---------------------------------------------------------------------
bool isPairingValid(const wchar_t* nucleus, const wchar_t* finalPart) {
    if (!finalPart || finalPart[0] == L'\0') return true;

    auto wcsEq = [](const wchar_t* a, const wchar_t* b) -> bool {
        while (*a && *b) {
            if (*a != *b) return false;
            ++a; ++b;
        }
        return *a == *b;
    };

    bool isNhCh = wcsEq(finalPart, L"nh") || wcsEq(finalPart, L"ch");
    bool isNgC  = wcsEq(finalPart, L"ng") || wcsEq(finalPart, L"c");

    if (isNhCh) {
        return wcsEq(nucleus, L"a")  || wcsEq(nucleus, L"i")
            || wcsEq(nucleus, L"\u00ea") /* ê */ || wcsEq(nucleus, L"y")
            || wcsEq(nucleus, L"oa") || wcsEq(nucleus, L"uy")
            || wcsEq(nucleus, L"u\u00ea") /* uê */;
    }
    if (isNgC) {
        if (wcsEq(nucleus, L"i") || wcsEq(nucleus, L"\u00ea") /* ê */
            || wcsEq(nucleus, L"y")) {
            return false;
        }
    }
    return true;
}

// Render một wstring thành utf-8 hex compact để in trong message lỗi.
std::string toHex(const std::wstring& s) {
    std::ostringstream os;
    os << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < s.size(); ++i) {
        if (i > 0) os << ' ';
        os << "U+" << std::setw(4)
           << static_cast<unsigned>(s[i]);
    }
    return os.str();
}

} // anonymous namespace

// =====================================================================
// [Feature: engine-core-refactor, Property 13:
//   Round-trip IsCompleteSyllable với pretty-printer]
// Validates: Requirements 17.2, 17.3, 17.4
//
// Forward direction (R17.2): Với MỌI bộ `(initial, nucleus, final)` mà
// pretty-printer sinh chuỗi `s` không vi phạm Nucleus-Final Pairing Rule,
// `IsCompleteSyllable(s, len(s))` SHALL trả `true`.
//
// Reverse direction (R17.3): Với MỌI chuỗi `s` mà
// `IsCompleteSyllable(s, len(s))` trả `false`, KHÔNG có bộ index hợp lệ
// nào sinh ra `s`. Ở chiều này, ta kiểm tra một corpus chuỗi rõ-ràng-vô-lệ
// (không initial Việt hợp lệ, hoặc thiếu nucleus, hoặc len > 20).
//
// Coverage (R17.4): pretty-printer enumerate ≥ 200 bộ đa dạng nucleus/final.
// =====================================================================
TEST_CASE("P13 forward: pretty-printed valid syllable -> IsCompleteSyllable == true") {
    constexpr int nInitials = sizeof(kInitials) / sizeof(kInitials[0]);
    constexpr int nNuclei   = sizeof(kNuclei)   / sizeof(kNuclei[0]);
    constexpr int nFinals   = sizeof(kFinals)   / sizeof(kFinals[0]);

    int validatedCount     = 0;
    int rejectedByPairing  = 0;

    for (int i = 0; i < nInitials; ++i) {
        for (int n = 0; n < nNuclei; ++n) {
            for (int f = 0; f < nFinals; ++f) {
                if (isAmbiguousInitialNucleus(kInitials[i], kNuclei[n])) {
                    ++rejectedByPairing;
                    continue;
                }
                if (!isPairingValid(kNuclei[n], kFinals[f])) {
                    ++rejectedByPairing;
                    continue;
                }

                std::wstring s = prettyPrintSyllable(
                    kInitials[i], kNuclei[n], kFinals[f], L"");

                bool ok = Cay::IsCompleteSyllable(
                    s.c_str(), static_cast<int>(s.size()));

                INFO("Pretty-printed syllable hex: " << toHex(s));
                INFO("initial=\"" << toHex(std::wstring(kInitials[i])) << "\", "
                     "nucleus=\"" << toHex(std::wstring(kNuclei[n]))   << "\", "
                     "final=\""   << toHex(std::wstring(kFinals[f]))   << "\"");
                REQUIRE(ok);
                ++validatedCount;
            }
        }
    }

    MESSAGE("P13 forward: validated=" << validatedCount
            << ", rejected by Pairing Rule=" << rejectedByPairing);

    // Yêu cầu coverage: ≥ 200 bộ index hợp lệ (Requirement 17.4).
    REQUIRE(validatedCount >= 200);
}

TEST_CASE("P13 reverse: invalid sequences -> IsCompleteSyllable == false") {
    // Mỗi entry là một chuỗi KHÔNG thể được pretty-printer sinh ra từ
    // bất kỳ bộ index hợp lệ nào.
    //   "zzz"   : 'z' không phải initial, nucleus, hay final.
    //   "bbn"   : 'b' khớp initial, nhưng 'b' không phải nucleus.
    //   "qz"    : 'q' đơn không phải initial (chỉ "qu"), 'q' không phải nucleus.
    //   "xyzab" : 'x' khớp initial, 'y' khớp nucleus, "zab" thừa.
    //   "kxq"   : 'k' khớp initial, 'x' không phải nucleus.
    //   "123"   : ký số, không khớp gì cả.
    //   ""      : len <= 0 → false (early return).
    //   "ng"    : 'ng' khớp initial, không có nucleus → false.
    //   "ing"   : nucleus 'i' + final 'ng' vi phạm Pairing Rule.
    //   "ach"   : nucleus 'a' + final 'ch' OK theo rule → đáng lẽ true,
    //             nên KHÔNG đưa vào (chỉ đưa vào những chuỗi chắc chắn
    //             invalid).
    static const wchar_t* const kInvalid[] = {
        L"zzz",
        L"bbn",
        L"qz",
        L"xyzab",
        L"kxq",
        L"123",
        L"",
        L"ng",
        L"ing",
    };
    constexpr int kInvalidCount = sizeof(kInvalid) / sizeof(kInvalid[0]);

    for (int i = 0; i < kInvalidCount; ++i) {
        const wchar_t* s = kInvalid[i];
        int len = 0;
        while (s[len]) ++len;

        bool result = Cay::IsCompleteSyllable(s, len);
        INFO("Invalid input hex: " << toHex(std::wstring(s)));
        REQUIRE_FALSE(result);
    }
}

// =====================================================================
// [Feature: engine-core-refactor, Property 16:
//   IsValidNucleus đúng trên corpus]
// Validates: Requirements 3.4, 3.5, 3.6
//
// Forward (R3.5): For each âm tiết Việt hợp lệ trong corpus, phần
// `expectedNucleus` (đã strip dấu thanh, lowercase, giữ dấu mũ/móc)
// SHALL được CayData::IsValidNucleus chấp nhận.
//
// Reverse (R3.6): Với corpus đối chiếu {yi, yo, yu, ou, ăn},
// CayData::IsValidNucleus SHALL từ chối.
//
// Lưu ý (R3.4): task 4.1 (Phase 3) đã single-source-of-truth-hoá
// `s_nuclei` và đồng thời làm sạch các entry rác (yi/yo/yu/ou/ăn). Vì
// vậy P16 phải pass ngay tại Phase 3, KHÔNG cần `DOCTEST_SKIP`.
// =====================================================================
TEST_CASE("P16 forward: corpus expectedNucleus -> IsValidNucleus == true") {
    int passed = 0;
    int skipped = 0;

    for (int i = 0; i < CayTestCorpus::g_vietnameseCorpusCount; ++i) {
        const wchar_t* nucleus =
            CayTestCorpus::g_vietnameseCorpus[i].expectedNucleus;
        if (!nucleus || nucleus[0] == L'\0') {
            ++skipped;
            continue;
        }

        int len = 0;
        while (nucleus[len]) ++len;

        bool ok = Cay::CayData::IsValidNucleus(nucleus, len);
        INFO("corpus[" << i << "] telex=\""
             << toHex(std::wstring(
                    CayTestCorpus::g_vietnameseCorpus[i].telexInput))
             << "\", nucleus=\"" << toHex(std::wstring(nucleus)) << "\"");
        REQUIRE(ok);
        ++passed;
    }

    MESSAGE("P16 forward: passed=" << passed << ", skipped(empty)=" << skipped);
    REQUIRE(passed > 0);
}

TEST_CASE("P16 reverse: garbage strings -> IsValidNucleus == false") {
    static const wchar_t* const kGarbageNuclei[] = {
        L"yi",
        L"yo",
        L"yu",
        L"ou",
        L"\u0103n", // ăn (vần có phụ âm cuối, không phải nucleus)
    };
    constexpr int kGarbageCount =
        sizeof(kGarbageNuclei) / sizeof(kGarbageNuclei[0]);

    for (int i = 0; i < kGarbageCount; ++i) {
        const wchar_t* g = kGarbageNuclei[i];
        int len = 0;
        while (g[len]) ++len;

        bool ok = Cay::CayData::IsValidNucleus(g, len);
        INFO("garbage nucleus hex: " << toHex(std::wstring(g)));
        REQUIRE_FALSE(ok);
    }
}
