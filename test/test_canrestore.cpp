// =====================================================================
// test/test_canrestore.cpp
//
// [Feature: engine-core-refactor, Property 14: Space + Backspace round-trip]
// Validates: Requirements 18.1, 18.2, 18.3
//
// Property 14: For any chuỗi phím tạo ra một âm tiết Telex hợp lệ và có
// `_textLen > 0`, sequence `[chuỗi phím] + Space + Backspace` SHALL khôi
// phục `_text`, `_toneIndex` về trạng thái ngay trước Space, đồng thời
// `_canRestore == false` sau Backspace (đã restore xong, nhánh kế tiếp
// nếu Backspace lần nữa SHALL không restore lại).
//
// Trace mô phỏng:
//   1. Type [chuỗi phím] → engine apply Telex transform → S1 = (text,
//      textLen, toneIndex, canRestore).
//   2. Type Space → CommitWord() → SaveState() (lưu _saved* + canRestore=true)
//      → ResetState() (text/buffer/toneIndex zero).
//   3. Type Backspace với _bufferCount==0 && _canRestore==true → restore
//      _buffer/_text/_toneIndex từ _saved* và set _canRestore=false → S2.
//   4. Assert S1.text == S2.text, S1.toneIndex == S2.toneIndex,
//      S2.canRestore == false.
//
// Generator: `cay::test::genTelexSyllable()` (test/property.h) sinh chuỗi
// Telex ASCII gồm initial+nucleus+final+tail.
//
// Iterations: 100 (theo .kiro/specs/engine-core-refactor/tasks.md task 8.4).
//
// Filter (precondition):
//   - `_textLen > 0` sau khi gõ (tasks.md yêu cầu rõ).
//   - Nếu `_text` có dấu tiếng Việt VÀ `IsCompleteSyllable` trả false trên
//     dạng tone-stripped lower-case → engine sẽ trigger `FallbackToRaw()`
//     ngay trong nhánh Space (xem CayEngine.cpp KeyCode::Space) khiến S2
//     mang text raw thay vì text S1 — Property 14 không phát biểu cho input
//     đó (Requirement 18 chỉ áp dụng cho âm tiết Telex hợp lệ). Skip để
//     giữ assertion sound.
// =====================================================================

#include "doctest.h"
#include "property.h"
#include "CayData.h"
#include "CayEngine.h"
#include "CayTypes.h"

#include <string>

namespace {

// Map 1 char ASCII alpha → Cay::KeyCode (KeyA..KeyZ định nghĩa = 'A'..'Z'
// trong CayTypes.h, nên uppercase được dùng làm enum-value).
Cay::KeyCode mapAlphaToKeyCode(wchar_t ch) {
    if (ch >= L'a' && ch <= L'z') {
        return static_cast<Cay::KeyCode>(L'A' + (ch - L'a'));
    }
    if (ch >= L'A' && ch <= L'Z') {
        return static_cast<Cay::KeyCode>(ch);
    }
    return Cay::KeyCode::Unknown;
}

void feedAlpha(Cay::TelexEngine& engine, wchar_t ch) {
    Cay::KeyEvent e{};
    e.character = ch;
    e.keyCode   = mapAlphaToKeyCode(ch);
    e.handled   = false;
    engine.OnKeyDown(e);
}

void feedSpace(Cay::TelexEngine& engine) {
    Cay::KeyEvent e{};
    e.character = L' ';
    e.keyCode   = Cay::KeyCode::Space;
    e.handled   = false;
    engine.OnKeyDown(e);
}

void feedBackspace(Cay::TelexEngine& engine) {
    Cay::KeyEvent e{};
    e.character = 0;
    e.keyCode   = Cay::KeyCode::Backspace;
    e.handled   = false;
    engine.OnKeyDown(e);
}

// So khớp byte-perfect phần `_text[0.._textLen)` giữa 2 snapshot.
bool textEqual(const Cay::DebugState& a, const Cay::DebugState& b) {
    if (a.textLen != b.textLen) return false;
    for (int i = 0; i < a.textLen; i++) {
        if (a.text[i] != b.text[i]) return false;
    }
    return true;
}

// Trả true nếu engine sẽ kích hoạt `FallbackToRaw()` ở nhánh Space (xem
// CayEngine.cpp KeyCode::Space). Mirror đúng logic trong engine để filter
// các iteration mà Property 14 không apply.
bool spaceWouldFallback(const Cay::DebugState& s) {
    if (!Cay::CayData::HasVietnameseMark(s.text, s.textLen)) return false;
    wchar_t textLo[Cay::MAX_BUFFER];
    for (int i = 0; i < s.textLen; i++) {
        textLo[i] = Cay::CayData::ToLowerViet(
                        Cay::CayData::StripTone(s.text[i]));
    }
    textLo[s.textLen] = L'\0';
    return !Cay::IsCompleteSyllable(textLo, s.textLen);
}

} // anonymous namespace

// =====================================================================
// Property 14 — Space + Backspace round-trip.
// 100 iterations theo spec (tasks.md task 8.4).
// Validates: Requirements 18.1, 18.2, 18.3.
// =====================================================================
TEST_CASE("P14: Space + Backspace round-trip restores text and toneIndex") {
    cay::test::forAll(cay::test::genTelexSyllable(), 100,
        [](const std::wstring& syllable) {
            Cay::TelexEngine engine;
            cay::test::MockInjector::reset();
            engine.OnInjectText = &cay::test::MockInjector::Hook;

            // 1. Gõ từng ký tự ASCII của syllable (generator chỉ sinh
            //    lowercase a..z; ký tự ngoài dải là sai contract, skip
            //    để robust).
            for (wchar_t c : syllable) {
                if (c < L'a' || c > L'z') continue;
                feedAlpha(engine, c);
            }

            // 2. Snapshot S1.
            Cay::DebugState s1 = engine.GetDebugState();

            // Precondition: _textLen > 0 (tasks.md task 8.4).
            if (s1.textLen <= 0) return;

            // Filter: nếu Space sẽ kích hoạt FallbackToRaw → engine cố ý
            // ghi đè _text trước commit để bảo vệ user khỏi commit từ
            // vô nghĩa có dấu (Requirement 18 KHÔNG cover input đó).
            if (spaceWouldFallback(s1)) return;

            // 3. Round-trip: Space → Backspace.
            feedSpace(engine);
            feedBackspace(engine);

            // 4. Snapshot S2 và assert.
            Cay::DebugState s2 = engine.GetDebugState();

            // Build a debuggable representation of the syllable and the
            // two text snapshots ngay khi assertion fail (INFO chỉ in
            // khi CHECK/REQUIRE bên dưới fail).
            std::string asciiSeq;
            asciiSeq.reserve(syllable.size());
            for (wchar_t c : syllable) {
                if (c >= 0 && c < 128) asciiSeq.push_back(static_cast<char>(c));
            }
            INFO("syllable=\"" << asciiSeq << "\"");
            INFO("S1.textLen=" << s1.textLen
                 << ", S2.textLen=" << s2.textLen);
            INFO("S1.toneIndex=" << s1.toneIndex
                 << ", S2.toneIndex=" << s2.toneIndex);
            INFO("S2.canRestore=" << (s2.canRestore ? 1 : 0));

            // R18.2: `_text` được khôi phục về trạng thái ngay trước Space.
            REQUIRE(textEqual(s1, s2));

            // R18.2: `_toneIndex` được khôi phục.
            REQUIRE(s1.toneIndex == s2.toneIndex);

            // R18.2: sau khi Backspace restore xong, `_canRestore` SHALL
            // = false (lần Backspace kế tiếp sẽ KHÔNG restore lại nữa,
            // ngăn double-restore).
            REQUIRE(s2.canRestore == false);
        });
}
