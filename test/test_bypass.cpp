// test/test_bypass.cpp
//
// [Feature: engine-core-refactor, Property 10: Bypass đúng cho corpus tiếng Anh và tiếng Việt]
// Validates: Requirements 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7
//
// Property:
//   - For each từ Anh trong g_englishCorpus: gõ chuỗi → assert
//     ShouldBypassWord() == true tại keystroke cuối.
//   - For each từ Việt trong g_vietnameseCorpus: gõ chuỗi → assert
//     ShouldBypassWord() == false tại keystroke cuối (trừ trường hợp
//     Strict Tone-Final Consonant Rule với f/r/x trên từ kết thúc bằng
//     c/ch/p/t — đó là behavior thiết kế của Requirement 8.5/8.6).
//
// ShouldBypassWord là private const method; truy cập qua test-only accessor
// TelexEngine::DebugShouldBypassWord() (gated #ifdef CAY_TEST_BUILD).

#include "doctest.h"
#include "property.h"
#include "corpus.h"
#include "CayEngine.h"
#include "CayTypes.h"

#include <string>

namespace {

// Type a sequence of "keys" (each char/wchar_t treated as a single keystroke)
// into a fresh engine. Resets MockInjector before typing so callers can
// inspect engine state right after the last keystroke.
//
// Generic over CharT so we can feed both `const char*` (English ASCII corpus)
// and `const wchar_t*` (Vietnamese telex input corpus) without converting.
template <typename CharT>
void typeIntoChars(Cay::TelexEngine& engine, const CharT* keys) {
    cay::test::MockInjector::reset();
    engine.OnInjectText = &cay::test::MockInjector::Hook;
    for (const CharT* p = keys; *p; ++p) {
        Cay::KeyEvent e{};
        wchar_t c = static_cast<wchar_t>(*p);
        e.character = c;
        // KeyCode dùng uppercase letter làm enum value (KeyA = 'A', ..., KeyZ = 'Z').
        wchar_t kc;
        if (c >= L'a' && c <= L'z')      kc = static_cast<wchar_t>(L'A' + (c - L'a'));
        else if (c >= L'A' && c <= L'Z') kc = c;
        else                             kc = c;
        e.keyCode = static_cast<Cay::KeyCode>(kc);
        e.handled = false;
        engine.OnKeyDown(e);
    }
}

// Strict Tone-Final Consonant Rule (Requirement 8.5): nếu telex input kết
// thúc bằng f/r/x VÀ phần còn lại (sau strip tone key) kết thúc bằng
// c/ch/p/t thì ShouldBypassWord trả true → đây là legitimate bypass cho
// từ Việt, không tính là failure.
bool isStrictToneFinalCase(const wchar_t* telex) {
    // Tìm len
    int len = 0;
    while (telex[len]) ++len;
    if (len < 2) return false;

    wchar_t lastKey = telex[len - 1];
    if (lastKey != L'f' && lastKey != L'r' && lastKey != L'x') return false;

    // Phần "before the trailing tone key" = telex[0..len-2)
    // Heuristic: nếu char trước tone key là c/p/t, hoặc 2 char trước là "ch", → strict rule.
    wchar_t prev1 = telex[len - 2];
    if (prev1 == L'c' || prev1 == L'p' || prev1 == L't') return true;
    if (len >= 3) {
        wchar_t prev2 = telex[len - 3];
        if (prev2 == L'c' && prev1 == L'h') return true;
    }
    return false;
}

} // anonymous namespace

// =====================================================================
// P10 — English corpus → ShouldBypassWord() phải trả true tại keystroke cuối.
// =====================================================================
TEST_CASE("P10: ShouldBypassWord — English words → true") {
    int verified = 0;
    int falsePositives = 0;
    for (int i = 0; i < CayTestCorpus::g_englishCorpusCount; i++) {
        const char* word = CayTestCorpus::g_englishCorpus[i].word;
        Cay::TelexEngine engine;
        typeIntoChars(engine, word);

        bool bypass = engine.DebugShouldBypassWord();
        if (!bypass) {
            falsePositives++;
        } else {
            verified++;
        }
        REQUIRE_MESSAGE(bypass,
            "English word `" << word << "` should be bypassed (R8.1-8.4) but "
            "ShouldBypassWord() returned false (engine treats as Vietnamese)");
    }
    MESSAGE("P10 English: " << verified << " bypassed, "
            << falsePositives << " false positives (out of "
            << CayTestCorpus::g_englishCorpusCount << ")");
}

// =====================================================================
// P10 — Vietnamese corpus → ShouldBypassWord() phải trả false tại keystroke
// cuối (trừ trường hợp Strict Tone-Final Consonant Rule, R8.5).
// =====================================================================
TEST_CASE("P10: ShouldBypassWord — Vietnamese words → false (trừ Strict Tone-Final)") {
    int correctlyNotBypassed = 0;
    int strictToneFinalBypassed = 0; // legitimate bypass per R8.5
    int unexpectedBypassed = 0;

    for (int i = 0; i < CayTestCorpus::g_vietnameseCorpusCount; i++) {
        const wchar_t* keys = CayTestCorpus::g_vietnameseCorpus[i].telexInput;
        Cay::TelexEngine engine;
        typeIntoChars(engine, keys);

        bool bypass = engine.DebugShouldBypassWord();
        bool isStrict = isStrictToneFinalCase(keys);

        if (!bypass) {
            correctlyNotBypassed++;
        } else if (isStrict) {
            strictToneFinalBypassed++;
        } else {
            unexpectedBypassed++;
            // Telex input của corpus chỉ chứa ASCII a..z + i/y/w/u/o/...,
            // nên widen→narrow chuyển 1-1 sang printable ASCII.
            std::string asAscii;
            for (const wchar_t* p = keys; *p; ++p) {
                asAscii.push_back(static_cast<char>(*p));
            }
            REQUIRE_MESSAGE(false,
                "Vietnamese word `" << asAscii
                << "` was bypassed but is NOT a Strict Tone-Final case (R8.5 exception)");
        }
    }

    MESSAGE("P10 Vietnamese: "
            << correctlyNotBypassed << " correctly NOT bypassed, "
            << strictToneFinalBypassed << " bypassed by Strict Tone-Final Rule (R8.5), "
            << unexpectedBypassed << " unexpected bypass (out of "
            << CayTestCorpus::g_vietnameseCorpusCount << ")");

    // Hard requirement: 0 unexpected bypass.
    REQUIRE_MESSAGE(unexpectedBypassed == 0,
        "Vietnamese corpus has " << unexpectedBypassed
        << " unexpected bypass cases (not Strict Tone-Final).");
}
