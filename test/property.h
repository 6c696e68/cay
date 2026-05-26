// =====================================================================
// test/property.h — Mini Property-Based Testing layer cho Cay engine
//
// Cung cấp:
//   * Gen<T>          — generator template, trả về T từ một seed.
//   * genVowelChar()  — sinh ngẫu nhiên 1 trong 144 ký tự nguyên âm
//                       tiếng Việt (12 base × 6 tone × 2 case).
//   * genTelexSyllable() — sinh chuỗi Telex (ASCII) gồm
//                       initial + nucleus + final + tail hợp lệ.
//   * genKeySeq(N)    — sinh chuỗi Cay::KeyEvent độ dài 1..N.
//   * forAll(gen, iters, pred) — runner chạy `iters` lần,
//                       seed mặc định 42, override qua env CAY_TEST_SEED.
//   * MockCall, MockInjector — mock cho Cay::InjectTextFunc, capture
//                       (backspaceCount, newText) vào std::vector.
//
// File này CHỈ dùng cho test build (`cay_test`), được phép xài STL.
// KHÔNG include từ src/core/.
// =====================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include "doctest.h"

#include "CayTypes.h"

namespace cay::test {

// ---------------------------------------------------------------------
// Seed: cố định 42, override qua env CAY_TEST_SEED (số thập phân).
// ---------------------------------------------------------------------
inline uint32_t getSeed() {
    if (const char* env = std::getenv("CAY_TEST_SEED")) {
        if (env[0] != '\0') {
            return static_cast<uint32_t>(std::strtoul(env, nullptr, 10));
        }
    }
    return 42u;
}

// ---------------------------------------------------------------------
// Gen<T>: generator giao diện. `sample(seed) -> T`.
// Dùng std::function để hỗ trợ closure (genKeySeq cần capture maxLen).
// ---------------------------------------------------------------------
template <typename T>
struct Gen {
    std::function<T(uint32_t)> sample;
};

// ---------------------------------------------------------------------
// Bảng 144 ký tự nguyên âm tiếng Việt (12 base × 6 tone × 2 case).
// Layout: 24 dòng × 6 ký tự = 144. Mỗi dòng = 1 base × 1 case × 6 tone.
//   row 0 : a (lower)        | row 1 : A (upper)
//   row 2 : â                | row 3 : Â
//   ... (tổng 12 base × 2 case = 24 row)
// ---------------------------------------------------------------------
namespace detail {

inline const wchar_t* allVowelChars() {
    // 12 base × 2 case = 24 chuỗi 6 ký tự, concat lại 144 ký tự.
    // Mỗi chuỗi: [base, huyền, sắc, hỏi, ngã, nặng]
    static const wchar_t kChars[] =
        // a / A
        L"a\u00e0\u00e1\u1ea3\u00e3\u1ea1"
        L"A\u00c0\u00c1\u1ea2\u00c3\u1ea0"
        // â / Â
        L"\u00e2\u1ea7\u1ea5\u1ea9\u1eab\u1ead"
        L"\u00c2\u1ea6\u1ea4\u1ea8\u1eaa\u1eac"
        // ă / Ă
        L"\u0103\u1eb1\u1eaf\u1eb3\u1eb5\u1eb7"
        L"\u0102\u1eb0\u1eae\u1eb2\u1eb4\u1eb6"
        // e / E
        L"e\u00e8\u00e9\u1ebb\u1ebd\u1eb9"
        L"E\u00c8\u00c9\u1eba\u1ebc\u1eb8"
        // ê / Ê
        L"\u00ea\u1ec1\u1ebf\u1ec3\u1ec5\u1ec7"
        L"\u00ca\u1ec0\u1ebe\u1ec2\u1ec4\u1ec6"
        // i / I
        L"i\u00ec\u00ed\u1ec9\u0129\u1ecb"
        L"I\u00cc\u00cd\u1ec8\u0128\u1eca"
        // o / O
        L"o\u00f2\u00f3\u1ecf\u00f5\u1ecd"
        L"O\u00d2\u00d3\u1ece\u00d5\u1ecc"
        // ô / Ô
        L"\u00f4\u1ed3\u1ed1\u1ed5\u1ed7\u1ed9"
        L"\u00d4\u1ed2\u1ed0\u1ed4\u1ed6\u1ed8"
        // ơ / Ơ
        L"\u01a1\u1edd\u1edb\u1edf\u1ee1\u1ee3"
        L"\u01a0\u1edc\u1eda\u1ede\u1ee0\u1ee2"
        // u / U
        L"u\u00f9\u00fa\u1ee7\u0169\u1ee5"
        L"U\u00d9\u00da\u1ee6\u0168\u1ee4"
        // ư / Ư
        L"\u01b0\u1eeb\u1ee9\u1eed\u1eef\u1ef1"
        L"\u01af\u1eea\u1ee8\u1eec\u1eee\u1ef0"
        // y / Y
        L"y\u1ef3\u00fd\u1ef7\u1ef9\u1ef5"
        L"Y\u1ef2\u00dd\u1ef6\u1ef8\u1ef4";
    // 24 chuỗi × 6 ký tự + 1 null = 145 phần tử.
    static_assert(sizeof(kChars) / sizeof(wchar_t) == 145,
                  "allVowelChars phải có đúng 144 ký tự + null terminator");
    return kChars;
}

constexpr int kVowelCharCount = 144;

// Bảng Telex ASCII cho generation (chuỗi raw user gõ — chưa transform).
// Nucleus chứa cả dạng đơn (a, e, i, ...) và double-key (aa, ee, oo, dd),
// hook (aw, ow, uw), và một số tổ hợp 2-3 nguyên âm phổ biến.
inline const std::vector<std::wstring>& initials() {
    static const std::vector<std::wstring> v = {
        L"",   L"b",   L"c",   L"ch", L"d",  L"dd", L"g",  L"gh",
        L"gi", L"h",   L"k",   L"kh", L"l",  L"m",  L"n",  L"ng",
        L"ngh",L"nh",  L"p",   L"ph", L"qu", L"r",  L"s",  L"t",
        L"th", L"tr",  L"v",   L"x"
    };
    return v;
}

inline const std::vector<std::wstring>& nuclei() {
    static const std::vector<std::wstring> v = {
        // 1 nguyên âm (kèm double/hook Telex)
        L"a",  L"aa", L"aw", L"e",  L"ee", L"i",  L"o",  L"oo",
        L"ow", L"u",  L"uw", L"y",
        // 2 nguyên âm
        L"ai",  L"ao",  L"au",  L"aau", L"aay", L"ay",
        L"eo",  L"eeu",
        L"ia",  L"ie",  L"iee", L"iu",
        L"oa",  L"oaw", L"oe",  L"oi",
        L"ua",  L"uaa", L"uee", L"ui",  L"uoo", L"uy",  L"uo", L"ue",
        L"uwa", L"uwi", L"uwu", L"uwow",
        L"ya",  L"yee", L"ye",
        // 3 nguyên âm phổ biến
        L"ieeu", L"yeeu", L"uwowu", L"uooi", L"uwowi",
        L"oai", L"oay",  L"uya",   L"uyee"
    };
    return v;
}

inline const std::vector<std::wstring>& finals() {
    static const std::vector<std::wstring> v = {
        L"", L"c", L"ch", L"m", L"n", L"ng", L"nh", L"p", L"t"
    };
    return v;
}

inline const std::vector<std::wstring>& tails() {
    static const std::vector<std::wstring> v = {
        L"", L"i", L"y", L"o", L"u"
    };
    return v;
}

} // namespace detail

// ---------------------------------------------------------------------
// genVowelChar: 1 trong 144 ký tự nguyên âm tiếng Việt.
// ---------------------------------------------------------------------
inline Gen<wchar_t> genVowelChar() {
    return Gen<wchar_t>{
        [](uint32_t seed) -> wchar_t {
            std::mt19937 rng(seed);
            std::uniform_int_distribution<int> dist(0, detail::kVowelCharCount - 1);
            return detail::allVowelChars()[dist(rng)];
        }
    };
}

// ---------------------------------------------------------------------
// genTelexSyllable: ghép initial + nucleus + final + tail (mỗi phần
// có thể rỗng trừ nucleus). Output là chuỗi Telex ASCII raw mà user gõ.
// ---------------------------------------------------------------------
inline Gen<std::wstring> genTelexSyllable() {
    return Gen<std::wstring>{
        [](uint32_t seed) -> std::wstring {
            std::mt19937 rng(seed);
            const auto& I = detail::initials();
            const auto& N = detail::nuclei();
            const auto& F = detail::finals();
            const auto& T = detail::tails();

            std::uniform_int_distribution<size_t> dI(0, I.size() - 1);
            std::uniform_int_distribution<size_t> dN(0, N.size() - 1);
            std::uniform_int_distribution<size_t> dF(0, F.size() - 1);
            std::uniform_int_distribution<size_t> dT(0, T.size() - 1);

            std::wstring out;
            out.reserve(8);
            out += I[dI(rng)];
            out += N[dN(rng)];
            // Final và tail loại trừ nhau trong âm tiết Việt; chọn 1 trong 3:
            // chỉ final, chỉ tail, hoặc cả hai rỗng.
            std::uniform_int_distribution<int> dPick(0, 2);
            int pick = dPick(rng);
            if (pick == 0) {
                out += F[dF(rng)];
            } else if (pick == 1) {
                out += T[dT(rng)];
            }
            return out;
        }
    };
}

// ---------------------------------------------------------------------
// genKeySeq: chuỗi Cay::KeyEvent độ dài [1, maxLen], phím alpha a..z,
// xác suất nhỏ chèn Space/Backspace để mô phỏng commit/restore.
// ---------------------------------------------------------------------
inline Gen<std::vector<Cay::KeyEvent>> genKeySeq(int maxLen) {
    if (maxLen < 1) maxLen = 1;
    return Gen<std::vector<Cay::KeyEvent>>{
        [maxLen](uint32_t seed) -> std::vector<Cay::KeyEvent> {
            std::mt19937 rng(seed);
            std::uniform_int_distribution<int> dLen(1, maxLen);
            std::uniform_int_distribution<int> dCat(0, 99);
            std::uniform_int_distribution<int> dAlpha(0, 25);
            std::uniform_int_distribution<int> dCase(0, 1);

            int n = dLen(rng);
            std::vector<Cay::KeyEvent> out;
            out.reserve(static_cast<size_t>(n));

            for (int i = 0; i < n; i++) {
                int cat = dCat(rng);
                Cay::KeyEvent e{};
                e.handled = false;
                if (cat < 90) {
                    int idx = dAlpha(rng);
                    bool upper = (dCase(rng) == 1);
                    wchar_t ch = static_cast<wchar_t>((upper ? L'A' : L'a') + idx);
                    e.character = ch;
                    e.keyCode = static_cast<Cay::KeyCode>(L'A' + idx);
                } else if (cat < 96) {
                    e.character = L' ';
                    e.keyCode = Cay::KeyCode::Space;
                } else {
                    e.character = 0;
                    e.keyCode = Cay::KeyCode::Backspace;
                }
                out.push_back(e);
            }
            return out;
        }
    };
}

// ---------------------------------------------------------------------
// forAll: chạy `pred` qua `iterations` mẫu sinh từ `gen`.
// Seed lấy từ getSeed(); mỗi iteration dùng seed con từ một rng cha.
// Pred: (const T&) -> void; bên trong dùng CHECK/REQUIRE của doctest.
// ---------------------------------------------------------------------
template <typename T, typename Pred>
void forAll(const Gen<T>& gen, int iterations, Pred pred) {
    std::mt19937 parent(getSeed());
    std::uniform_int_distribution<uint32_t> dist;
    for (int i = 0; i < iterations; i++) {
        uint32_t childSeed = dist(parent);
        T value = gen.sample(childSeed);
        pred(value);
    }
}

// ---------------------------------------------------------------------
// MockInjector: mock cho Cay::InjectTextFunc.
//
// Mỗi lần engine gọi callback (backspaceCount, newText, newTextLen),
// MockInjector::Hook lưu lại thành 1 entry MockCall trong vector tĩnh.
//
// Dùng:
//   engine.OnInjectText = &cay::test::MockInjector::Hook;
//   cay::test::MockInjector::reset();
//   ... gõ phím ...
//   const auto& trace = cay::test::MockInjector::calls();
// ---------------------------------------------------------------------
struct MockCall {
    int backspaceCount;
    std::wstring newText;
};

struct MockInjector {
    inline static std::vector<MockCall> s_calls;

    // Signature phải khớp với Cay::InjectTextFunc.
    static void Hook(int backspaceCount, const wchar_t* newText, int newTextLen) {
        std::wstring s;
        if (newText != nullptr && newTextLen > 0) {
            s.assign(newText, static_cast<size_t>(newTextLen));
        }
        s_calls.push_back(MockCall{backspaceCount, std::move(s)});
    }

    static void reset() { s_calls.clear(); }

    static const std::vector<MockCall>& calls() { return s_calls; }
};

// Đảm bảo signature của Hook tương thích với Cay::InjectTextFunc.
static_assert(std::is_same_v<decltype(&MockInjector::Hook), Cay::InjectTextFunc>,
              "MockInjector::Hook phải khớp signature Cay::InjectTextFunc");

} // namespace cay::test
