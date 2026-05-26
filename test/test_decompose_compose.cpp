// test/test_decompose_compose.cpp
//
// Property 1..4 — round-trip Decompose/Compose, completeness của
// GetToneMark, và inverse mapping của GetToneIndex.
//
// Spec: .kiro/specs/engine-core-refactor/tasks.md task 2.4..2.7
// Validates: Requirements 4.1, 4.2, 4.3, 9.1, 9.3, 9.4, 9.5

#include "doctest.h"
#include "property.h"

#include "CayData.h"

#include <iomanip>

// =====================================================================
// [Feature: engine-core-refactor, Property 1: Round-trip Decompose/Compose ký tự]
// Validates: Requirements 4.1, 4.2, 4.3
//
// For any ký tự `c` thuộc tập nguyên âm tiếng Việt được Engine_Core hỗ trợ
// (12 base × 6 tone × 2 case = 144 ký tự — sinh bởi genVowelChar()):
//   auto d = CayData::DecomposeChar(c);
//   REQUIRE(CayData::ComposeChar(d.base, d.toneIndex, d.isUpper) == c);
// =====================================================================
TEST_CASE("P1: Round-trip Decompose/Compose") {
    cay::test::forAll(cay::test::genVowelChar(), 200, [](wchar_t c) {
        Cay::DecomposedChar d = Cay::CayData::DecomposeChar(c);
        wchar_t composed = Cay::CayData::ComposeChar(d.base, d.toneIndex, d.isUpper);
        REQUIRE_MESSAGE(composed == c,
            "Round-trip Decompose/Compose thất bại cho U+"
            << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
            << static_cast<int>(static_cast<unsigned>(c))
            << ": decomposed (base=U+"
            << std::setw(4) << std::setfill('0')
            << static_cast<int>(static_cast<unsigned>(d.base))
            << ", tone=" << std::dec << d.toneIndex
            << ", upper=" << (d.isUpper ? "true" : "false")
            << ") rồi compose lại ra U+"
            << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
            << static_cast<int>(static_cast<unsigned>(composed)));
    });
}

// =====================================================================
// [Feature: engine-core-refactor, Property 2: Round-trip StripTone/GetToneMark]
// Validates: Requirements 9.1, 9.5
//
// For each base `b` ∈ {a, â, ă, e, ê, i, o, ô, ơ, u, ư, y} (lower + upper)
// và toneIndex 1..5: nếu c = GetToneMark(b, toneIndex) != 0 thì
//   REQUIRE(CayData::StripTone(c) == b);
//
// Lưu ý: GetToneMark switch trên lowercase base; với upper-case base ta
// lowercase trước, gọi GetToneMark, rồi ToUpperViet kết quả. StripTone có
// đủ entry cả lower- và upper-with-tone nên giữ nguyên case của input.
// =====================================================================
TEST_CASE("P2: Round-trip StripTone/GetToneMark") {
    static const wchar_t kBasesLower[12] = {
        L'a', L'\u00E2', L'\u0103', L'e', L'\u00EA', L'i',
        L'o', L'\u00F4', L'\u01A1', L'u', L'\u01B0', L'y'
    };
    static const wchar_t kBasesUpper[12] = {
        L'A', L'\u00C2', L'\u0102', L'E', L'\u00CA', L'I',
        L'O', L'\u00D4', L'\u01A0', L'U', L'\u01AF', L'Y'
    };

    for (int i = 0; i < 12; ++i) {
        for (int caseIdx = 0; caseIdx < 2; ++caseIdx) {
            const wchar_t b      = (caseIdx == 0) ? kBasesLower[i] : kBasesUpper[i];
            const wchar_t bLower = kBasesLower[i];
            for (int t = 1; t <= 5; ++t) {
                wchar_t markedLower = Cay::CayData::GetToneMark(bLower, t);
                if (markedLower == 0) continue; // không có mapping → bỏ qua

                wchar_t marked = (caseIdx == 0)
                                 ? markedLower
                                 : Cay::CayData::ToUpperViet(markedLower);
                wchar_t stripped = Cay::CayData::StripTone(marked);

                REQUIRE_MESSAGE(stripped == b,
                    "StripTone(GetToneMark(U+"
                    << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned>(b))
                    << ", " << std::dec << t << ")) = U+"
                    << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned>(stripped))
                    << ", kỳ vọng U+"
                    << std::setw(4) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned>(b)));
            }
        }
    }
}

// =====================================================================
// [Feature: engine-core-refactor, Property 3: Completeness của GetToneMark]
// Validates: Requirements 9.3
//
// For each base `b` ∈ {a, â, ă, e, ê, i, o, ô, ơ, u, ư, y} (lower) và
// toneIndex ∈ {1, 2, 3, 4, 5}:
//   REQUIRE(CayData::GetToneMark(b, toneIndex) != 0);
// =====================================================================
TEST_CASE("P3: Completeness của GetToneMark") {
    static const wchar_t kBases[12] = {
        L'a', L'\u00E2', L'\u0103', L'e', L'\u00EA', L'i',
        L'o', L'\u00F4', L'\u01A1', L'u', L'\u01B0', L'y'
    };
    static const char* const kToneNames[6] = {
        "ngang", "huyền", "sắc", "hỏi", "ngã", "nặng"
    };

    for (int i = 0; i < 12; ++i) {
        for (int t = 1; t <= 5; ++t) {
            wchar_t marked = Cay::CayData::GetToneMark(kBases[i], t);
            REQUIRE_MESSAGE(marked != 0,
                "GetToneMark(U+"
                << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                << static_cast<int>(static_cast<unsigned>(kBases[i]))
                << ", " << std::dec << t
                << " — " << kToneNames[t]
                << ") trả 0 (thiếu mapping cho base + tone)");
        }
    }
}

// =====================================================================
// [Feature: engine-core-refactor, Property 4: GetToneIndex inverse mapping]
// Validates: Requirements 9.4
//
// REQUIRE(CayData::GetToneIndex(L'z') == 0);
// REQUIRE(CayData::GetToneIndex(L'f') == 1);
// REQUIRE(CayData::GetToneIndex(L's') == 2);
// REQUIRE(CayData::GetToneIndex(L'r') == 3);
// REQUIRE(CayData::GetToneIndex(L'x') == 4);
// REQUIRE(CayData::GetToneIndex(L'j') == 5);
// — Tương tự cho uppercase. Phím khác trả -1.
// =====================================================================
TEST_CASE("P4: GetToneIndex inverse mapping") {
    // Lowercase Telex tone keys.
    REQUIRE(Cay::CayData::GetToneIndex(L'z') == 0);
    REQUIRE(Cay::CayData::GetToneIndex(L'f') == 1);
    REQUIRE(Cay::CayData::GetToneIndex(L's') == 2);
    REQUIRE(Cay::CayData::GetToneIndex(L'r') == 3);
    REQUIRE(Cay::CayData::GetToneIndex(L'x') == 4);
    REQUIRE(Cay::CayData::GetToneIndex(L'j') == 5);

    // Uppercase tone keys (Caps Lock / Shift) — cùng index.
    REQUIRE(Cay::CayData::GetToneIndex(L'Z') == 0);
    REQUIRE(Cay::CayData::GetToneIndex(L'F') == 1);
    REQUIRE(Cay::CayData::GetToneIndex(L'S') == 2);
    REQUIRE(Cay::CayData::GetToneIndex(L'R') == 3);
    REQUIRE(Cay::CayData::GetToneIndex(L'X') == 4);
    REQUIRE(Cay::CayData::GetToneIndex(L'J') == 5);

    // Phím alpha không phải tone-key trả -1.
    REQUIRE(Cay::CayData::GetToneIndex(L'a') == -1);
    REQUIRE(Cay::CayData::GetToneIndex(L'b') == -1);
    REQUIRE(Cay::CayData::GetToneIndex(L'q') == -1);
    REQUIRE(Cay::CayData::GetToneIndex(L'w') == -1);
    REQUIRE(Cay::CayData::GetToneIndex(L'A') == -1);
    REQUIRE(Cay::CayData::GetToneIndex(L'W') == -1);

    // Ký tự không phải Latin alpha cũng trả -1.
    REQUIRE(Cay::CayData::GetToneIndex(L' ') == -1);
    REQUIRE(Cay::CayData::GetToneIndex(L'0') == -1);
    REQUIRE(Cay::CayData::GetToneIndex(L'\0') == -1);
}
