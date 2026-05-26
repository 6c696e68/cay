// test/test_tone_position.cpp
//
// [Feature: engine-core-refactor, Property 9: Đặt dấu thanh đúng vị trí
//  theo quy tắc tiếng Việt]
// Validates: Requirements 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8
//
// Property: với mỗi âm tiết Việt hợp lệ trong corpus có annotate vị trí dấu
// kỳ vọng `expectedTonePos`, set engine `_text` = `expectedOutput` (trạng thái
// post-transform), gọi `FindTonePosition()` SHALL trả về đúng `expectedTonePos`.
//
// Tại sao dùng DebugSetText thay vì gõ telexInput rồi snapshot:
//   - FindTonePosition là pure function trên _text. Set _text trực tiếp cô
//     lập đối tượng test khỏi chuỗi Apply* (ApplyDoubleKeys/Hook/Tone).
//   - Nếu Apply* có bug, P9 vẫn pass cho input đúng; bug đó thuộc P5..P10
//     hoặc golden trace P15.
//   - Tránh phụ thuộc vào việc engine reproduce 100% expectedOutput từ
//     telexInput cho mọi entry corpus (đó là việc của P15).
//
// Coverage 7 cases tone-position rules (Requirements 7.1..7.7):
//   1) 1 nguyên âm                                    → tại nguyên âm đó
//   2) 2 nguyên âm + final consonant                  → nguyên âm thứ 2
//   3) 2 nguyên âm mở thuộc {oa, oe, uê, uy, uơ, iê}  → nguyên âm thứ 2
//   4) 2 nguyên âm mở khác                             → nguyên âm thứ 1
//   5) 3 nguyên âm + final consonant                   → nguyên âm cuối
//   6) 3 nguyên âm mở (trừ uyê/giuô/giươ)              → nguyên âm giữa
//   7) qu/gi + nguyên âm                               → nguyên âm sau q/g

#include "doctest.h"
#include "corpus.h"
#include "CayEngine.h"

#include <string>

using CayTestCorpus::g_vietnameseCorpus;
using CayTestCorpus::g_vietnameseCorpusCount;

namespace {

// Compute độ dài chuỗi wchar_t kết thúc null (không dùng CRT để khớp chuẩn
// no-CRT của engine; ở đây chỉ là tiện ích test).
int wlen(const wchar_t* s) {
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

} // anonymous namespace

TEST_CASE("P9: FindTonePosition đúng vị trí dấu thanh theo quy tắc tiếng Việt") {
    int withTone = 0;     // số entry corpus có dấu thanh (expectedTonePos >= 0)
    int verified = 0;     // số entry assert pass

    for (int i = 0; i < g_vietnameseCorpusCount; i++) {
        const auto& entry = g_vietnameseCorpus[i];

        // Skip entry không có dấu thanh — FindTonePosition sẽ trả vị trí
        // ứng viên (vẫn có nghĩa) nhưng không có ground truth để so sánh.
        if (entry.expectedTonePos < 0) continue;
        withTone++;

        Cay::TelexEngine engine;
        const int len = wlen(entry.expectedOutput);
        engine.DebugSetText(entry.expectedOutput, len);

        const int actual = engine.DebugFindTonePosition();
        const int expected = entry.expectedTonePos;

        // Dùng REQUIRE để fail-fast với đúng entry sai (dễ debug). Nếu cần
        // thấy hết các case fail trong 1 lần chạy thì đổi sang CHECK.
        std::wstring textCopy(entry.expectedOutput, len);
        std::string textUtf8;
        textUtf8.reserve(static_cast<size_t>(len) * 3);
        for (wchar_t c : textCopy) {
            // Naive ASCII-only encoder cho diagnostics (đủ để in ra index/codepoint).
            if (c < 0x80) {
                textUtf8.push_back(static_cast<char>(c));
            } else {
                textUtf8.push_back('?');
            }
        }

        REQUIRE_MESSAGE(actual == expected,
            "P9 entry #" << i
            << " telex=`" << textUtf8 << "`"
            << " expectedTonePos=" << expected
            << " actual=" << actual);

        verified++;
    }

    // Sanity: corpus phải có đủ số entry có dấu thanh để test đáng giá.
    // Theo gen_corpus.py corpus có ~150 entry tone (75% của 200).
    REQUIRE_MESSAGE(withTone > 100,
        "Corpus phải có > 100 entry có dấu thanh (got " << withTone << ")");
    REQUIRE_MESSAGE(verified == withTone,
        "Số entry verified phải bằng số entry có dấu thanh (verified="
        << verified << ", withTone=" << withTone << ")");
}
