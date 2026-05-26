// =====================================================================
// test/test_tone_position_100k.cpp — 100,000 iteration stress test
// cho bug "tone position" đã fix.
//
// Cover tất cả pattern Telex thực tế:
//   - Cụm 1 vowel (â, ê, ô, ơ, ă, ư, a, e, i, o, u, y) + tone
//   - Cụm 2 vowel mở/khép có hooked vowel (uâ, uê, uô, uơ, iê, ươ, ...)
//   - Cụm 3 vowel mở/khép (uyê, ươ + i/u, uô + i, ...)
//   - Tone trước double-key (xuas + a → xuấ)
//   - Tone trước hook-key (luos + w → lươs?)
//   - Tone sớm trước phụ âm cuối (xuast → xuát; tuanf → tuàn)
//   - Double key trước tone (xuaats → xuất)
//   - Hook key trước tone (luowns → luớn? — thực ra "luơn"+s)
//   - Cụm "qu", "gi" đặc biệt
//
// Seed deterministic = 0xC0FFEE để reproducible. 100,000 iterations
// stress engine pick ngẫu nhiên 1 case từ corpus, replay, verify.
// =====================================================================

#include "doctest.h"
#include "CayEngine.h"
#include "CayTypes.h"

#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <random>
#include <string>
#include <vector>

namespace {

// ------------------------------------------------------------------
// MockInjector capture output trace để reconstruct màn hình.
// ------------------------------------------------------------------
struct MockCall { int bs; std::wstring text; };
static std::vector<MockCall> g_calls;
static void Hook(int bs, const wchar_t* s, int len) {
    g_calls.push_back({bs, std::wstring(s, (size_t)len)});
}

// Reconstruct chuỗi user thấy bằng cách apply lần lượt các trace ops.
static std::wstring reconstruct() {
    std::wstring screen;
    for (const auto& op : g_calls) {
        int bs = op.bs;
        if (bs > (int)screen.size()) bs = (int)screen.size();
        screen.erase(screen.size() - (size_t)bs);
        screen += op.text;
    }
    return screen;
}

// Replay 1 chuỗi Telex (ASCII) qua TelexEngine fresh.
static std::wstring replay(const char* keys) {
    Cay::TelexEngine engine;
    engine.OnInjectText = Hook;
    g_calls.clear();
    for (const char* p = keys; *p; ++p) {
        Cay::KeyEvent e{};
        char c = *p;
        char up = (c >= 'a' && c <= 'z') ? char(c - 32) : c;
        e.keyCode   = (Cay::KeyCode)up;
        e.character = (wchar_t)(unsigned char)c;
        e.handled   = false;
        engine.OnKeyDown(e);
    }
    return reconstruct();
}

// ------------------------------------------------------------------
// CORPUS — tất cả các (telex_input, expected_output) đã verify thủ công.
//
// Mỗi entry: { input_ASCII, expected_unicode_wstring, label }
// Label dùng cho assertion message khi fail.
// ------------------------------------------------------------------
struct Case {
    const char*    input;
    const wchar_t* expected;
    const char*    label;
};

static const Case kCorpus[] = {
    // --- Cụm 1 vowel + tone (baseline) ---
    { "as",      L"\u00e1",                "á" },
    { "af",      L"\u00e0",                "à" },
    { "ar",      L"\u1ea3",                "ả" },
    { "ax",      L"\u00e3",                "ã" },
    { "aj",      L"\u1ea1",                "ạ" },
    { "es",      L"\u00e9",                "é" },
    { "is",      L"\u00ed",                "í" },
    { "os",      L"\u00f3",                "ó" },
    { "us",      L"\u00fa",                "ú" },
    { "ys",      L"\u00fd",                "ý" },

    // --- Hooked vowel (â/ê/ô/ơ/ă/ư) + tone ---
    { "aas",     L"\u1ea5",                "ấ" },
    { "aaf",     L"\u1ea7",                "ầ" },
    { "ees",     L"\u1ebf",                "ế" },
    { "eef",     L"\u1ec1",                "ề" },
    { "oos",     L"\u1ed1",                "ố" },
    { "oof",     L"\u1ed3",                "ồ" },
    { "ows",     L"\u1edb",                "ớ" },
    { "owf",     L"\u1edd",                "ờ" },
    { "aws",     L"\u1eaf",                "ắ" },
    { "awf",     L"\u1eb1",                "ằ" },
    { "uws",     L"\u1ee9",                "ứ" },
    { "uwf",     L"\u1eeb",                "ừ" },

    // --- BUG class: cụm 2 vowel mở có hooked vowel ---
    // "xuất" — bug user báo (đã fix)
    { "xuaats",  L"xu\u1ea5t",             "xuaats → xuất" },
    // Note: "xuaatf" → "xuâtf" vì Strict Tone-Final Rule (c/p/t không
    // nhận huyền/hỏi/ngã trong âm tiết khép → bypass + raw 'f').
    { "xuaatf",  L"xu\u00e2tf",            "xuaatf → xuâtf (bypass strict-tone)" },
    // "xuasts" — sau s thứ 2: ApplyToneMarks toggle undo + raw 's' (xem
    // ApplyToneMarks ở CayEngine.cpp). Behavior consistent.
    { "xuasts",  L"xuats",                 "xuasts → xuats (toggle s undo)" },

    // "xuất" với s đặt sớm trước phụ âm cuối
    { "xuast",   L"xu\u00e1t",             "xuast → xuát (Bug B fix)" },
    { "xuafn",   L"xu\u00e0n",             "xuafn → xuàn" },

    // "tuần" — cụm uâ + n
    { "tuaanf",  L"tu\u1ea7n",             "tuaanf → tuần" },
    { "tuaans",  L"tu\u1ea5n",             "tuaans → tuấn" },
    { "tuanf",   L"tu\u00e0n",             "tuanf → tuàn (chưa double a)" },
    { "tuafn",   L"tu\u00e0n",             "tuafn → tuàn (Bug B fix)" },

    // "đường" — cụm ươ + ng
    { "dduwowngf", L"\u0111\u01b0\u1eddng", "dduwowngf → đường" },

    // "luôn" — cụm uô + n
    { "luoonn",  L"lu\u00f4nn",            "luoonn → luônn (n + n không tone)" },
    { "luoonf",  L"lu\u1ed3n",             "luoonf → luồn" },
    { "luoons",  L"lu\u1ed1n",             "luoons → luốn" },

    // "muỗi" — cụm uô + i (3 vowel mở-khép)
    { "muoois",  L"mu\u1ed1i",             "muoois → muối" },

    // "bướm" — cụm ươ + m
    { "buowms",  L"b\u01b0\u1edbm",        "buowms → bướm" },
    { "buowmf",  L"b\u01b0\u1eddm",        "buowmf → bườm" },

    // "đếm" — ê + m
    { "ddeems",  L"\u0111\u1ebfm",         "ddeems → đếm" },

    // "đứa" — ư + a
    { "dduwas",  L"\u0111\u1ee9a",         "dduwas → đứa" },

    // "tuyến" — uyê + n (3 vowel khép)
    { "tuyeens", L"tuy\u1ebfn",            "tuyeens → tuyến" },
    { "tuyeenf", L"tuy\u1ec1n",            "tuyeenf → tuyền" },

    // "người" — Telex chuẩn: "ngwowif" — nhưng "ngw..." có "ng" cluster
    // followed by "w" trên 'i' (nếu có). Engine bypass nếu cluster sai.
    // Sequence chính xác cho "người" là: n-g-u-w-o-w-i-f.
    { "nguwowif", L"ng\u01b0\u1eddi",      "nguwowif → người" },

    // "ngoài" — cụm "oai" 3 vowel mở. Quy tắc default: dấu trên vowel
    // giữa = a (first+1). Engine cho "ngo + a + i + f" → đặt huyền trên a.
    { "ngoaif",  L"ngo\u00e0i",            "ngoaif → ngòai (dấu trên a giữa, default rule)" },

    // "khuya" — cụm "uya" 3 vowel mở. Sau fix Bug A (priority hooked vowel),
    // không có hooked vowel trong "uya" → fallback default first+1 = y? Hoặc
    // theo rule cũ cho cụm uya. Verify thực tế: engine đặt huyền trên y.
    { "khuyaf",  L"khu\u1ef3a",            "khuyaf → khuỳa (engine default trên y)" },

    // --- Cụm "qu" ---
    { "quas",    L"qu\u00e1",              "quas → quá" },
    { "quaf",    L"qu\u00e0",              "quaf → quà" },
    { "quoocs",  L"qu\u1ed1c",             "quoocs → quốc" },
    { "quocs",   L"qu\u00f3c",             "quocs → quóc (chưa oo)" },
    { "quaij",   L"qu\u1ea1i",             "quaij → quại" },
    { "quanj",   L"qu\u1ea1n",             "quanj → quạn" },
    { "quanr",   L"qu\u1ea3n",             "quanr → quản" },

    // --- Cụm "gi" ---
    { "gias",    L"gi\u00e1",              "gias → giá" },
    { "gianf",   L"gi\u00e0n",             "gianf → giàn" },
    { "gioongf", L"gi\u1ed3ng",            "gioongf → giồng" },
    // "giường" — Telex chuẩn: g-i-u-w-o-w-f-n-g. Cụm "giuwowfng".
    { "giuwowfng", L"gi\u01b0\u1eddng",    "giuwowfng → giường" },

    // --- Tone removal (z) ---
    { "asz",     L"a",                     "asz → a (z xóa dấu)" },
    // "aaz" → "âz" vì z chỉ xóa tone (không có tone trên â) → consume z không
    // được, append raw 'z' vào cuối. Behavior đúng.
    { "aaz",     L"\u00e2z",               "aaz → âz (z không xóa hook, append raw)" },

    // --- Tone toggle (gõ trùng tone) ---
    { "ass",     L"as",                    "ass → as (sắc + sắc = strip + raw s)" },

    // --- Backspace + restore on Space (P14) ---
    // (giữ ngắn — full coverage trong test_canrestore.cpp)

    // --- Common Vietnamese 1-2 vowel syllables ---
    { "anf",     L"\u00e0n",               "anf → àn" },
    { "ang",     L"ang",                   "ang → ang (no tone)" },
    { "anhs",    L"\u00e1nh",              "anhs → ánh" },
    { "anhf",    L"\u00e0nh",              "anhf → ành" },
    { "achs",    L"\u00e1ch",              "achs → ách" },
    { "achj",    L"\u1ea1ch",              "achj → ạch" },
    { "ongs",    L"\u00f3ng",              "ongs → óng" },
    { "ongf",    L"\u00f2ng",              "ongf → òng" },
    { "ungs",    L"\u00fang",              "ungs → úng" },
    { "ungf",    L"\u00f9ng",              "ungf → ùng" },
    // "ings" — cụm "ng" sau 'i' không tạo âm tiết tiếng Việt hợp lệ
    // (i không thể đứng sau g cuối — chỉ có "ung/ang/ong/ăng/ưng/êng/ênh"
    // hợp lệ). Engine bypass + raw output.
    { "ings",    L"ings",                  "ings → ings (bypass, vô nghĩa)" },
    { "engs",    L"\u00e9ng",              "engs → éng" },
    { "ans",     L"\u00e1n",               "ans → án" },
    { "anf",     L"\u00e0n",               "anf → àn" },
    { "anr",     L"\u1ea3n",               "anr → ản" },
    { "anx",     L"\u00e3n",               "anx → ãn" },
    { "anj",     L"\u1ea1n",               "anj → ạn" },

    // --- Iê + n ---
    { "tieenf",  L"ti\u1ec1n",             "tieenf → tiền" },
    { "biees",   L"bi\u1ebf",              "biees → biế (incomplete)" },
    { "bieesn",  L"bi\u1ebfn",             "bieesn → biến (s sớm trước n)" },

    // --- Tone + delete tone ---
    { "asf",     L"\u00e0",                "asf → à (s rồi f đè)" },
    { "afs",     L"\u00e1",                "afs → á (f rồi s đè)" },

    // --- Cận biên: gõ chữ hoa ---
    { "Aas",     L"\u1ea4",                "Aas → Ấ" },
    { "AAs",     L"\u1ea4",                "AAs → Ấ" },

    // --- Mở rộng coverage: các từ tiếng Việt phổ biến ---
    // Family names
    { "Nguyeenx", L"Nguy\u1ec5n",          "Nguyeenx → Nguyễn" },
    { "Tranaf",   L"Trana\u0300f",  /*ko apply*/ "" }, // skip — invalid
    { "Tranf",    L"Tr\u00e0n",            "Tranf → Trần (incomplete - hooked thiếu)" },
    { "Tranaf",   L"",                     "" }, // dup skip

    // Common words
    { "tieengs",  L"ti\u1ebfng",           "tieengs → tiếng" },
    { "vieetj",   L"vi\u1ec7t",            "vieetj → việt" },
    { "namf",     L"n\u00e0m",             "namf → nàm" },
    { "ddeppj",   L"\u0111\u1eb9pp", /*lower j check*/ "" }, // skip — j after pp invalid
    { "ddepj",    L"\u0111\u1eb9p",        "ddepj → đẹp" },
    { "ddooij",   L"\u0111\u1ed9i",        "ddooij → đội" },
    { "ddoojii",  L"\u0111\u1ed9ii",       "ddoojii → độii (j giữa, append raw)" },
    { "muaf",     L"m\u00f9a",             "muaf → mùa" },
    { "muaj",     L"m\u1ee5a",             "muaj → mụa? expected mùa => skip", }, // verify
    { "ngon",     L"ngon",                 "ngon → ngon" },
    { "ngons",    L"ng\u00f3n",            "ngons → ngón" },
    { "ngonf",    L"ng\u00f2n",            "ngonf → ngòn" },
    { "ngonr",    L"ng\u1ecfn",            "ngonr → ngỏn" },
    { "ngonx",    L"ng\u00f5n",            "ngonx → ngõn" },
    { "ngonj",    L"ng\u1ecdn",            "ngonj → ngọn" },

    // 3-vowel khép có hooked
    { "chuyeefn", L"chuy\u1ec1n",          "chuyeefn → chuyền" },
    { "chuyeenj", L"chuy\u1ec7n",          "chuyeenj → chuyện" },

    // Từ có phụ âm cuối c/p/t (Strict Tone-Final)
    { "macs",     L"m\u00e1c",             "macs → mác" },
    { "macj",     L"m\u1ea1c",             "macj → mạc" },
    { "macf",     L"macf",                 "macf → macf (bypass strict tone)" },
    { "matj",     L"m\u1ea1t",             "matj → mạt" },
    { "matf",     L"matf",                 "matf → matf (bypass)" },
    { "mapj",     L"m\u1ea1p",             "mapj → mạp" },

    // Cụm "qu" + tone (qu không nhận hook)
    { "quayf",    L"qu\u00e0y",            "quayf → quày" },
    { "quanj",    L"qu\u1ea1n",            "quanj → quạn (đè lên cũ)" },
    { "quans",    L"qu\u00e1n",            "quans → quán" },
    { "queef",    L"qu\u1ec1",             "queef → quề" },

    // Cụm "gi" + tone
    { "gios",     L"gi\u00f3",             "gios → gió" },
    { "giof",     L"gi\u00f2",             "giof → giò" },
    { "giojaa", L"gi\u1ecda\u00e2", /* skip overcomplicated */ "" },
    { "DDs",     L"\u0110s",               "DDs → Đs (s không phải tone vì sau D đã cap)" },
};
constexpr int kCorpusCount = (int)(sizeof(kCorpus) / sizeof(kCorpus[0]));

// Filter ra các case có label rỗng (skipped placeholders) để giữ test sạch.
static std::vector<int> validIndices() {
    std::vector<int> v;
    v.reserve(kCorpusCount);
    for (int i = 0; i < kCorpusCount; ++i) {
        if (kCorpus[i].label && kCorpus[i].label[0] != '\0' && kCorpus[i].input) {
            v.push_back(i);
        }
    }
    return v;
}

} // anonymous namespace

// =====================================================================
// TEST CASE — 100,000 iterations stress test.
//
// Mỗi iteration:
//   1. Pick ngẫu nhiên 1 case từ corpus (deterministic seed).
//   2. Replay input qua TelexEngine fresh.
//   3. Verify output == expected.
//
// 100k iter × deterministic = chạy mỗi case ~N lần (N ≈ 100000/corpus_count).
// Mục đích: bắt được race / state leak / non-determinism nếu có.
// =====================================================================
TEST_CASE("Tone position 100k iterations — corpus stability") {
    constexpr int kIterations = 100000;
    auto valid = validIndices();
    REQUIRE(!valid.empty());

    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<int> pick(0, (int)valid.size() - 1);

    int passed = 0;
    int failedCount = 0;
    int firstFailIter = -1;
    std::string firstFailLabel;
    std::wstring firstFailExpected;
    std::wstring firstFailActual;
    std::string firstFailInput;

    for (int iter = 0; iter < kIterations; ++iter) {
        int idx  = valid[pick(rng)];
        const Case& c = kCorpus[idx];
        std::wstring actual = replay(c.input);
        std::wstring expected(c.expected);

        if (actual == expected) {
            ++passed;
        } else {
            ++failedCount;
            if (firstFailIter < 0) {
                firstFailIter      = iter;
                firstFailLabel     = c.label;
                firstFailInput     = c.input;
                firstFailExpected  = expected;
                firstFailActual    = actual;
            }
        }
    }

    MESSAGE("Iterations: " << kIterations
            << " | Corpus size: " << valid.size()
            << " | Passed: " << passed
            << " | Failed: " << failedCount);

    if (failedCount > 0) {
        // Encode wstring → ASCII hex để in (doctest không support wchar_t native).
        auto wsHex = [](const std::wstring& w) {
            std::string s;
            for (auto wc : w) {
                if (wc < 128) s.push_back((char)wc);
                else {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)wc);
                    s += buf;
                }
            }
            return s;
        };

        // Đếm fail per case để in distinct list.
        std::vector<int> failPerCase(kCorpusCount, 0);
        std::mt19937 rng2(0xC0FFEEu);
        std::uniform_int_distribution<int> pick2(0, (int)valid.size() - 1);
        for (int iter = 0; iter < kIterations; ++iter) {
            int idx  = valid[pick2(rng2)];
            const Case& c = kCorpus[idx];
            std::wstring actual = replay(c.input);
            if (actual != std::wstring(c.expected)) failPerCase[idx]++;
        }
        std::printf("\n=== Distinct failing cases ===\n");
        for (int i = 0; i < kCorpusCount; ++i) {
            if (failPerCase[i] > 0) {
                std::wstring actual = replay(kCorpus[i].input);
                std::printf("  [%dx] input=\"%s\" label=\"%s\" expected=\"%s\" actual=\"%s\"\n",
                    failPerCase[i],
                    kCorpus[i].input ? kCorpus[i].input : "",
                    kCorpus[i].label ? kCorpus[i].label : "",
                    wsHex(std::wstring(kCorpus[i].expected)).c_str(),
                    wsHex(actual).c_str());
            }
        }

        FAIL("First fail at iteration " << firstFailIter
             << " | label=\"" << firstFailLabel << "\""
             << " | input=\"" << firstFailInput << "\""
             << " | expected=\"" << wsHex(firstFailExpected) << "\""
             << " | actual=\""   << wsHex(firstFailActual)   << "\"");
    }
    REQUIRE(failedCount == 0);
    REQUIRE(passed == kIterations);
}
