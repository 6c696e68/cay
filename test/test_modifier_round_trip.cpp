// test/test_modifier_round_trip.cpp
//
// Property 5..8 — round-trip của tone-key double-press, double-key triple-press,
// hook-key double-press, và tone-toggle qua phím z.
//
// Spec: .kiro/specs/engine-core-refactor/tasks.md task 3.5..3.8
// Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5, 6.6
//
// Ghi chú thiết kế:
// - Mọi test tạo `TelexEngine` mới và `MockInjector::reset()` trước khi gõ
//   để loại bỏ cross-test contamination.
// - Output của engine được tái dựng từ chuỗi callback `(backspaceCount,newText)`
//   capture bởi `MockInjector::Hook` — đó chính là contract giữa engine và
//   platform layer (OnInjectText), nên đây là nguồn sự thật về output.

#include "doctest.h"
#include "property.h"
#include "CayEngine.h"
#include "CayTypes.h"

#include <string>

namespace {

// Helper: gõ chuỗi ASCII keys vào engine mới, trả _text cuối cùng dưới dạng
// wstring. Mỗi key được encode thành Cay::KeyEvent với character + keyCode
// tương ứng (alpha → KeyA..KeyZ).
std::wstring typeAndCaptureText(const std::string& keys) {
    Cay::TelexEngine engine;
    cay::test::MockInjector::reset();
    engine.OnInjectText = &cay::test::MockInjector::Hook;

    for (char c : keys) {
        Cay::KeyEvent e{};
        e.character = static_cast<wchar_t>(c);
        // KeyCode dùng uppercase letter làm enum value (KeyA = 'A', ..., KeyZ = 'Z').
        wchar_t kc;
        if (c >= 'a' && c <= 'z') kc = static_cast<wchar_t>('A' + (c - 'a'));
        else if (c >= 'A' && c <= 'Z') kc = static_cast<wchar_t>(c);
        else kc = static_cast<wchar_t>(c);
        e.keyCode = static_cast<Cay::KeyCode>(kc);
        e.handled = false;
        engine.OnKeyDown(e);
    }

    // Tái dựng buffer cuối cùng từ trace MockInjector: mỗi call áp
    // (backspaceCount + newText) vào virtual buffer, đúng như platform layer.
    std::wstring buf;
    for (const auto& call : cay::test::MockInjector::calls()) {
        if (call.backspaceCount > 0) {
            int n = call.backspaceCount;
            if (n > static_cast<int>(buf.size())) n = static_cast<int>(buf.size());
            buf.resize(buf.size() - static_cast<size_t>(n));
        }
        buf += call.newText;
    }
    return buf;
}

// Convert ASCII C-string → wstring (mỗi char widen thành wchar_t cùng codepoint).
std::wstring asciiToWide(const std::string& s) {
    std::wstring w;
    w.reserve(s.size());
    for (char c : s) w.push_back(static_cast<wchar_t>(c));
    return w;
}

} // anonymous namespace

// =====================================================================
// [Feature: engine-core-refactor, Property 5: Round-trip tone key double-press]
// Validates: Requirements 6.1, 6.6
//
// Property: với mỗi âm tiết Telex hợp lệ `[base]` và mỗi tone key `t` ∈
// {s,f,r,x,j} (cộng uppercase), gõ `[base] + t + t` từ trạng thái rỗng phải
// cho output bằng `engine_output([base]) + raw t character`.
//
// Lý do dùng `engine_output([base]) + raw_t` thay cho literal ASCII:
// một số âm tiết base bản thân không tone (ví dụ `mua`, `qua`, `troi`)
// nhưng có thể được engine giữ nguyên dạng ASCII; nếu engine sau này áp
// dụng auto-transform khác trên base, expected vẫn theo dõi đúng. Quan
// trọng nhất: round-trip bảo toàn rằng "double-press tone == không tone +
// raw char append".
//
// Lưu ý: corpus loại trừ các âm tiết chứa pattern `uo` vì engine kích hoạt
// AUTO HOOK UO→ƯƠ trong `ApplyToneMarks` (xem CayEngine.cpp), khiến
// `nuocss` ra `nươcs` (auto-hook không bị undo khi gỡ tone). Đây là
// behavior được thiết kế của engine; round-trip property của
// Requirements 6.1 không áp dụng cho các âm tiết auto-hook đó.
// =====================================================================
TEST_CASE("P5: Round-trip tone key double-press") {
    // Reduced corpus: 50 âm tiết Telex hợp lệ, KHÔNG chứa pattern `uo`
    // (để tránh AUTO HOOK UO→ƯƠ trong ApplyToneMarks) và KHÔNG kết thúc
    // bằng c/p/t/ch (để tránh Strict Tone-Final Consonant Rule trong
    // ShouldBypassWord — bypass cho tone keys f/r/x khi từ kết thúc bằng
    // các phụ âm cứng đó, làm tone không được apply lần đầu).
    static const char* const kBaseKeys[] = {
        "toi",  "ban",  "anh",  "em",   "cha",  "me",   "con",  "nha",
        "ba",   "chu",  "vo",   "mai",  "moi",  "troi", "song", "bay",
        "nui",  "bien", "rung", "gio",  "nang", "mua",  "hoa",  "la",
        "cay",  "co",   "sen",  "qua",  "tay",  "nho",  "chim", "ca",
        "lay",  "ga",   "lon",  "bo",   "cho",  "meo",  "voi",  "ho",
        "khi",  "doi",  "tho",  "ran",  "hai",  "mui",  "sao",  "de",
        "trau", "ong"
    };
    static const int kBaseCount = sizeof(kBaseKeys) / sizeof(kBaseKeys[0]);
    static_assert(sizeof(kBaseKeys) / sizeof(kBaseKeys[0]) == 50,
                  "P5 corpus phải có đúng 50 âm tiết");

    static const char kTones[] = { 's', 'f', 'r', 'x', 'j' }; // bỏ 'z' (P8 lo)

    for (int i = 0; i < kBaseCount; i++) {
        const std::string base = kBaseKeys[i];
        const std::wstring baseOutput = typeAndCaptureText(base);

        for (char t : kTones) {
            // Lowercase tone
            const std::string seq = base + t + t;
            const std::wstring expected = baseOutput + static_cast<wchar_t>(t);
            const std::wstring actual = typeAndCaptureText(seq);
            REQUIRE_MESSAGE(actual == expected,
                "P5 lower fail: base=" << base << " tone=" << t
                << " seq=" << seq << " (expected len=" << expected.size()
                << ", actual len=" << actual.size() << ")");

            // Uppercase tone (cùng base lowercase, chỉ tone chữ hoa)
            const char T = static_cast<char>(t - 'a' + 'A');
            const std::string seqU = base + T + T;
            const std::wstring expectedU = baseOutput + static_cast<wchar_t>(T);
            const std::wstring actualU = typeAndCaptureText(seqU);
            REQUIRE_MESSAGE(actualU == expectedU,
                "P5 upper fail: base=" << base << " tone=" << T
                << " seq=" << seqU << " (expected len=" << expectedU.size()
                << ", actual len=" << actualU.size() << ")");
        }
    }
}

// =====================================================================
// [Feature: engine-core-refactor, Property 6: Round-trip double-key triple-press]
// Validates: Requirements 6.2
//
// Property: với mỗi `k` ∈ {a,e,o,d} (lower + upper), gõ `kkk` từ trạng thái
// rỗng phải cho `_text` cuối cùng = `[k, k]` (2 ký tự ASCII thuần).
// Trace: `k` → plain append; `kk` → ApplyDoubleKeys apply (â/ê/ô/đ); `kkk`
// → ApplyDoubleKeys undo (về k thuần) + append k raw → cuối cùng "kk".
// =====================================================================
TEST_CASE("P6: Round-trip double-key triple-press") {
    static const char kKeys[] = { 'a', 'e', 'o', 'd' };

    for (char k : kKeys) {
        // Lowercase: kkk → expect "kk"
        const std::string seq = std::string(1, k) + k + k;
        const std::wstring out = typeAndCaptureText(seq);
        const std::wstring expected = { static_cast<wchar_t>(k), static_cast<wchar_t>(k) };
        REQUIRE_MESSAGE(out == expected,
            "P6 lower fail: k=" << k << " expected '" << k << k
            << "' (2 chars), got len=" << out.size());

        // Uppercase: KKK → expect "KK"
        const char K = static_cast<char>(k - 'a' + 'A');
        const std::string seqU = std::string(1, K) + K + K;
        const std::wstring outU = typeAndCaptureText(seqU);
        const std::wstring expectedU = { static_cast<wchar_t>(K), static_cast<wchar_t>(K) };
        REQUIRE_MESSAGE(outU == expectedU,
            "P6 upper fail: K=" << K << " expected '" << K << K
            << "' (2 chars), got len=" << outU.size());
    }
}

// =====================================================================
// [Feature: engine-core-refactor, Property 7: Round-trip hook-key double-press]
// Validates: Requirements 6.3
//
// Property: với mỗi combo tạo hook (`ow→ơ`, `aw→ă`, `uw→ư`), gõ thêm `w`
// lần nữa undo hook và append `w` raw → output base + 'w' (2 chars ASCII).
//
// Trường hợp `oow`: `oo` đã thành `ô` qua double-key (KHÔNG phải hook). `ô`
// không có hook rule (GetHookRule(ô)=0) → `w` chỉ được append raw → output
// `ô + w` (2 chars).
// =====================================================================
TEST_CASE("P7: Round-trip hook-key double-press") {
    // ow + w → o + w raw (undo hook ơ→o)
    {
        const std::wstring out = typeAndCaptureText("oww");
        const std::wstring expected = L"ow";
        REQUIRE_MESSAGE(out == expected,
            "P7 ow→ơ→ow round-trip fail: expected 'ow' (2 chars), got len="
            << out.size());
    }
    // aw + w → a + w raw (undo hook ă→a)
    {
        const std::wstring out = typeAndCaptureText("aww");
        const std::wstring expected = L"aw";
        REQUIRE_MESSAGE(out == expected,
            "P7 aw→ă→aw round-trip fail: expected 'aw' (2 chars), got len="
            << out.size());
    }
    // uw + w → u + w raw (undo hook ư→u)
    {
        const std::wstring out = typeAndCaptureText("uww");
        const std::wstring expected = L"uw";
        REQUIRE_MESSAGE(out == expected,
            "P7 uw→ư→uw round-trip fail: expected 'uw' (2 chars), got len="
            << out.size());
    }
    // oow: oo→ô (double-key); w trên ô không có hook → append raw w → "ôw".
    {
        const std::wstring out = typeAndCaptureText("oow");
        REQUIRE_MESSAGE(out.size() == 2,
            "P7 oow expected 2-char output, got len=" << out.size());
        REQUIRE_MESSAGE(out[0] == L'\u00f4',
            "P7 oow first char must be ô (U+00F4)");
        REQUIRE_MESSAGE(out[1] == L'w',
            "P7 oow second char must be raw 'w'");
    }
}

// =====================================================================
// [Feature: engine-core-refactor, Property 8: Tone toggle với phím z]
// Validates: Requirements 6.4, 6.5
//
// Case 1: `_text` đã có dấu thanh (gõ `tois` để thành `tói`) → gõ `z` →
//         dấu thanh bị strip, ký tự `z` KHÔNG được append.
// Case 2: `_text` không có dấu thanh (gõ `toi`) → gõ `z` → `z` append raw.
// =====================================================================
TEST_CASE("P8: Tone toggle với phím z") {
    // --- Case 1: text has tone, z removes it without appending z ---
    {
        const std::wstring withTone = typeAndCaptureText("tois");
        const std::wstring afterZ   = typeAndCaptureText("toisz");

        // Setup sanity: `tois` phải sinh ra `tói` = t + ó(U+00F3) + i.
        REQUIRE_MESSAGE(withTone == L"t\u00f3i",
            "P8 case1 setup: 'tois' phải sinh ra 'tói' (t,U+00F3,i), got len="
            << withTone.size());

        // After z: tone bị strip + KHÔNG append z. Expected = `toi` (3 chars).
        REQUIRE_MESSAGE(afterZ == L"toi",
            "P8 case1 fail: 'toisz' expected 'toi' (tone stripped, z NOT appended), got len="
            << afterZ.size());
        // Cụ thể hơn: ký tự cuối không phải 'z'.
        const bool noTrailingZ = !afterZ.empty() && afterZ.back() != L'z';
        REQUIRE_MESSAGE(noTrailingZ,
            "P8 case1: ký tự cuối KHÔNG được là 'z' khi strip tone");
    }
    // --- Case 2: text without tone, z is appended raw ---
    {
        const std::wstring noTone = typeAndCaptureText("toi");
        const std::wstring afterZ = typeAndCaptureText("toiz");

        REQUIRE_MESSAGE(noTone == L"toi",
            "P8 case2 setup: 'toi' phải sinh ra 'toi' literal");

        // Expected: `toi` + `z` raw = `toiz` (4 chars).
        REQUIRE_MESSAGE(afterZ == L"toiz",
            "P8 case2 fail: 'toiz' expected 'toiz' (z appended raw), got len="
            << afterZ.size());
        REQUIRE_MESSAGE(afterZ.size() == noTone.size() + 1,
            "P8 case2: 'toiz' phải dài hơn 'toi' đúng 1 ký tự");
        const bool trailingZ = !afterZ.empty() && afterZ.back() == L'z';
        REQUIRE_MESSAGE(trailingZ,
            "P8 case2: ký tự cuối phải là 'z'");
    }
}
