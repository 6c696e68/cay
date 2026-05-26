// =====================================================================
// test/test_idempotency.cpp
//
// [Feature: engine-core-refactor, Property 11: Idempotency của reset operations]
// Validates: Requirements 10.1, 10.2
//
// Property 11: For any trạng thái engine đạt được sau một chuỗi KeyEvent
// ngẫu nhiên:
//   - Gọi StripAllTones() hai lần liên tiếp SHALL có _text[] không đổi
//     giữa lần 1 và lần 2.
//   - Gọi ResetState() hai lần liên tiếp SHALL có toàn bộ field nội bộ
//     bằng nhau giữa lần 1 và lần 2.
//
// File này CHỈ chạy trong build test (BUILD_TESTING=ON, định nghĩa
// CAY_TEST_BUILD). Property 12 (task 8.3) sẽ append TEST_CASE riêng vào
// cùng file này.
// =====================================================================

#include "doctest.h"
#include "property.h"
#include "CayEngine.h"
#include "CayTypes.h"

namespace {

// Field-by-field equality cho Cay::DebugState. Viết tay (không operator==
// trong header public) để tránh thay đổi public API của src/core/.
bool equalState(const Cay::DebugState& a, const Cay::DebugState& b) {
    if (a.bufferCount      != b.bufferCount)      return false;
    if (a.textLen          != b.textLen)          return false;
    if (a.toneIndex        != b.toneIndex)        return false;
    if (a.lastOutputLen    != b.lastOutputLen)    return false;
    if (a.savedBufferCount != b.savedBufferCount) return false;
    if (a.savedTextLen     != b.savedTextLen)     return false;
    if (a.savedToneIndex   != b.savedToneIndex)   return false;
    if (a.canRestore       != b.canRestore)       return false;

    for (int i = 0; i < Cay::MAX_BUFFER; i++) {
        if (a.buffer[i].raw     != b.buffer[i].raw)     return false;
        if (a.buffer[i].output  != b.buffer[i].output)  return false;
        if (a.text[i]           != b.text[i])           return false;
        if (a.lastOutput[i]     != b.lastOutput[i])     return false;
        if (a.savedBuffer[i].raw    != b.savedBuffer[i].raw)    return false;
        if (a.savedBuffer[i].output != b.savedBuffer[i].output) return false;
        if (a.savedText[i]      != b.savedText[i])     return false;
    }
    return true;
}

// So sánh chỉ phần _text[] (cho property StripAllTones — operation này
// chỉ chạm vào _text[], các field khác không đổi giữa 2 lần gọi).
bool equalText(const Cay::DebugState& a, const Cay::DebugState& b) {
    if (a.textLen != b.textLen) return false;
    for (int i = 0; i < Cay::MAX_BUFFER; i++) {
        if (a.text[i] != b.text[i]) return false;
    }
    return true;
}

// Type chuỗi KeyEvent vào engine từ trạng thái rỗng. Reset MockInjector
// trước, set callback. Sau khi return, engine state phản ánh kết quả gõ.
void typeKeySeq(Cay::TelexEngine& engine,
                const std::vector<Cay::KeyEvent>& keys) {
    cay::test::MockInjector::reset();
    engine.OnInjectText = &cay::test::MockInjector::Hook;
    for (Cay::KeyEvent e : keys) {
        e.handled = false;
        engine.OnKeyDown(e);
    }
}

} // anonymous namespace

// =====================================================================
// P11.a — StripAllTones() là idempotent: gọi 2 lần ⇒ _text[] không đổi.
// =====================================================================
TEST_CASE("P11: StripAllTones idempotent") {
    cay::test::forAll(cay::test::genKeySeq(16), 100,
        [](const std::vector<Cay::KeyEvent>& keys) {
            Cay::TelexEngine engine;
            typeKeySeq(engine, keys);

            // S1: state sau khi gõ.
            Cay::DebugState s1 = engine.GetDebugState();

            // Gọi StripAllTones() lần 1 → S2.
            engine.DebugStripAllTones();
            Cay::DebugState s2 = engine.GetDebugState();

            // Gọi StripAllTones() lần 2 → S3.
            engine.DebugStripAllTones();
            Cay::DebugState s3 = engine.GetDebugState();

            // Idempotency: S2 == S3 trên _text[] (operation chỉ chạm
            // _text). Đồng thời để chặt hơn, cả state đầy đủ cũng phải
            // bằng nhau (StripAllTones không thay đổi field nào khác).
            REQUIRE(equalText(s2, s3));
            REQUIRE(equalState(s2, s3));

            // Sanity: sau StripAllTones, _text không còn dấu thanh
            // (so với S1 có thể khác — chỉ check S1 KHÔNG được mất gì
            // ngoài tone, bằng cách kiểm tra textLen không đổi).
            REQUIRE(s1.textLen == s2.textLen);
        });
}

// =====================================================================
// P11.b — ResetState() là idempotent: gọi 2 lần ⇒ toàn bộ field bằng nhau.
// =====================================================================
TEST_CASE("P11: ResetState idempotent") {
    cay::test::forAll(cay::test::genKeySeq(16), 100,
        [](const std::vector<Cay::KeyEvent>& keys) {
            Cay::TelexEngine engine;
            typeKeySeq(engine, keys);

            // Gọi ResetState() lần 1 → S2.
            engine.DebugResetState();
            Cay::DebugState s2 = engine.GetDebugState();

            // Gọi ResetState() lần 2 → S3.
            engine.DebugResetState();
            Cay::DebugState s3 = engine.GetDebugState();

            // Idempotency: toàn bộ field nội bộ bằng nhau.
            REQUIRE(equalState(s2, s3));

            // Sanity: ResetState() phải đã zero các counter chính
            // (Requirement 10.2: "_bufferCount, _textLen, _toneIndex,
            // _lastOutputLen bằng nhau giữa lần 1 và lần 2"). Ở đây
            // kiểm tra giá trị cụ thể sau reset.
            REQUIRE(s2.bufferCount   == 0);
            REQUIRE(s2.textLen       == 0);
            REQUIRE(s2.toneIndex     == -1);
            REQUIRE(s2.lastOutputLen == 0);
        });
}

// =====================================================================
// P12 — ResetFull() tương đương khởi tạo mới.
//
// Validates: Requirements 10.3
//
// Với mọi chuỗi KeyEvent ngẫu nhiên gõ trên `e1`, sau khi gọi
// `e1.ResetFull()` thì toàn bộ state nội bộ của `e1` SHALL bằng state
// của một engine `e2` vừa khởi tạo mặc định (chưa nhận input nào).
// So sánh field-by-field qua `equalState()`.
// =====================================================================
TEST_CASE("P12: ResetFull equivalent to fresh construction") {
    cay::test::forAll(cay::test::genKeySeq(16), 100,
        [](const std::vector<Cay::KeyEvent>& keys) {
            Cay::TelexEngine e1, e2;
            typeKeySeq(e1, keys);
            e1.ResetFull();
            REQUIRE(equalState(e1.GetDebugState(), e2.GetDebugState()));
        });
}
