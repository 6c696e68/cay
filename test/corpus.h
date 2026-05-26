// =============================================================================
// test/corpus.h — Test corpus cho engine-core-refactor
//
// Auto-generated bởi test/gen_corpus.py — KHÔNG sửa file này trực tiếp.
// Sửa danh sách entries trong gen_corpus.py rồi chạy lại generator.
//
// Phục vụ:
//   - Property 9  (FindTonePosition) — Requirements 7.1..7.8
//   - Property 10 (ShouldBypassWord) — Requirements 8.1..8.7
//   - Property 16 (IsValidNucleus)   — Requirements 3.4..3.6
//
// Convention: mọi ký tự Unicode dùng \uXXXX escape (design.md Testing Strategy).
// Header-only, không include STL — phù hợp với test build.
// =============================================================================
#ifndef CAY_TEST_CORPUS_H
#define CAY_TEST_CORPUS_H

namespace CayTestCorpus {

// ---------------------------------------------------------------------------
// VietnameseEntry: 1 entry corpus tiếng Việt với annotation phục vụ test.
//   - telexInput      : chuỗi phím người dùng gõ (ASCII).
//   - expectedOutput  : chuỗi Unicode kỳ vọng sau khi engine xử lý.
//   - expectedNucleus : phần nucleus (đã strip dấu thanh, lowercase,
//                       giữ dấu mũ/móc — vd "trường" → "ươ").
//   - expectedTonePos : index trong expectedOutput của ký tự mang dấu thanh,
//                       hoặc -1 nếu không có dấu thanh.
// ---------------------------------------------------------------------------
struct VietnameseEntry {
    const wchar_t* telexInput;
    const wchar_t* expectedOutput;
    const wchar_t* expectedNucleus;
    int            expectedTonePos;
};

// ---------------------------------------------------------------------------
// EnglishEntry: 1 từ tiếng Anh kỳ vọng được bypass (ShouldBypassWord==true).
// ---------------------------------------------------------------------------
struct EnglishEntry {
    const char* word;
};

// ---------------------------------------------------------------------------
// 200 từ tiếng Việt — coverage:
//   - Group A: 100 từ phổ thông tần suất cao
//   - Group B: 50  tổ hợp dấu khó (mơ/mờ/mở/mỡ, hươu, nguyễn, giường, ...)
//   - Group C: 50  từ test FindTonePosition (1/2/3 nguyên âm, có/không cuối)
// ---------------------------------------------------------------------------
static const VietnameseEntry g_vietnameseCorpus[200] = {
    { L"tooi", L"t\u00F4i", L"\u00F4i", -1 },
    { L"banj", L"b\u1EA1n", L"a", 1 },
    { L"anh", L"anh", L"a", -1 },
    { L"chij", L"ch\u1ECB", L"i", 2 },
    { L"em", L"em", L"e", -1 },
    { L"cha", L"cha", L"a", -1 },
    { L"mej", L"m\u1EB9", L"e", 1 },
    { L"con", L"con", L"o", -1 },
    { L"oong", L"\u00F4ng", L"\u00F4", -1 },
    { L"baf", L"b\u00E0", L"a", 1 },
    { L"chus", L"ch\u00FA", L"u", 2 },
    { L"vowj", L"v\u1EE3", L"\u01A1", 1 },
    { L"choongf", L"ch\u1ED3ng", L"\u00F4", 2 },
    { L"baanj", L"b\u1EADn", L"\u00E2", 1 },
    { L"dduwas", L"\u0111\u1EE9a", L"\u01B0a", 1 },
    { L"nhaf", L"nh\u00E0", L"a", 2 },
    { L"nuwowcs", L"n\u01B0\u1EDBc", L"\u01B0\u01A1", 2 },
    { L"ddaats", L"\u0111\u1EA5t", L"\u00E2", 1 },
    { L"trowif", L"tr\u1EDDi", L"\u01A1i", 2 },
    { L"soong", L"s\u00F4ng", L"\u00F4", -1 },
    { L"suoois", L"su\u1ED1i", L"u\u00F4", 2 },
    { L"nuis", L"n\u00FAi", L"ui", 1 },
    { L"bieenr", L"bi\u1EC3n", L"i\u00EA", 2 },
    { L"ruwngf", L"r\u1EEBng", L"\u01B0", 1 },
    { L"ddoongf", L"\u0111\u1ED3ng", L"\u00F4", 1 },
    { L"gios", L"gi\u00F3", L"o", 2 },
    { L"nawngs", L"n\u1EAFng", L"\u0103", 1 },
    { L"muwa", L"m\u01B0a", L"\u01B0a", -1 },
    { L"tuyeets", L"tuy\u1EBFt", L"uy\u00EA", 3 },
    { L"baox", L"b\u00E3o", L"ao", 1 },
    { L"suwowng", L"s\u01B0\u01A1ng", L"\u01B0\u01A1", -1 },
    { L"maay", L"m\u00E2y", L"\u00E2y", -1 },
    { L"saams", L"s\u1EA5m", L"\u00E2", 1 },
    { L"bawng", L"b\u0103ng", L"\u0103", -1 },
    { L"nongs", L"n\u00F3ng", L"o", 1 },
    { L"hoa", L"hoa", L"oa", -1 },
    { L"las", L"l\u00E1", L"a", 1 },
    { L"caay", L"c\u00E2y", L"\u00E2y", -1 },
    { L"cor", L"c\u1ECF", L"o", 1 },
    { L"sen", L"sen", L"e", -1 },
    { L"quar", L"qu\u1EA3", L"a", 2 },
    { L"buwowir", L"b\u01B0\u1EDFi", L"\u01B0\u01A1i", 2 },
    { L"chuoois", L"chu\u1ED1i", L"u\u00F4", 3 },
    { L"ddaof", L"\u0111\u00E0o", L"ao", 1 },
    { L"nho", L"nho", L"o", -1 },
    { L"chim", L"chim", L"i", -1 },
    { L"cas", L"c\u00E1", L"a", 1 },
    { L"vitj", L"v\u1ECBt", L"i", 1 },
    { L"gaf", L"g\u00E0", L"a", 1 },
    { L"lownj", L"l\u1EE3n", L"\u01A1", 1 },
    { L"bof", L"b\u00F2", L"o", 1 },
    { L"nguwaj", L"ng\u1EF1a", L"\u01B0a", 2 },
    { L"chos", L"ch\u00F3", L"o", 2 },
    { L"meof", L"m\u00E8o", L"eo", 1 },
    { L"chuootj", L"chu\u1ED9t", L"u\u00F4", 3 },
    { L"roongf", L"r\u1ED3ng", L"\u00F4", 1 },
    { L"voi", L"voi", L"oi", -1 },
    { L"hoor", L"h\u1ED5", L"\u00F4", 1 },
    { L"gaaus", L"g\u1EA5u", L"\u00E2u", 1 },
    { L"khir", L"kh\u1EC9", L"i", 2 },
    { L"socs", L"s\u00F3c", L"o", 1 },
    { L"thor", L"th\u1ECF", L"o", 2 },
    { L"rawns", L"r\u1EAFn", L"\u0103", 1 },
    { L"eechs", L"\u1EBFch", L"\u00EA", 0 },
    { L"oocs", L"\u1ED1c", L"\u00F4", 0 },
    { L"saos", L"s\u00E1o", L"ao", 1 },
    { L"dee", L"d\u00EA", L"\u00EA", -1 },
    { L"traau", L"tr\u00E2u", L"\u00E2u", -1 },
    { L"buwowms", L"b\u01B0\u1EDBm", L"\u01B0\u01A1", 2 },
    { L"ong", L"ong", L"o", -1 },
    { L"cowm", L"c\u01A1m", L"\u01A1", -1 },
    { L"banhs", L"b\u00E1nh", L"a", 1 },
    { L"phowr", L"ph\u1EDF", L"\u01A1", 2 },
    { L"buns", L"b\u00FAn", L"u", 1 },
    { L"mif", L"m\u00EC", L"i", 1 },
    { L"thitj", L"th\u1ECBt", L"i", 2 },
    { L"xooi", L"x\u00F4i", L"\u00F4i", -1 },
    { L"ruwowuj", L"r\u01B0\u1EE3u", L"\u01B0\u01A1", 2 },
    { L"traf", L"tr\u00E0", L"a", 2 },
    { L"ddor", L"\u0111\u1ECF", L"o", 1 },
    { L"vangf", L"v\u00E0ng", L"a", 1 },
    { L"xanh", L"xanh", L"a", -1 },
    { L"tims", L"t\u00EDm", L"i", 1 },
    { L"trawngs", L"tr\u1EAFng", L"\u0103", 2 },
    { L"dden", L"\u0111en", L"e", -1 },
    { L"lowns", L"l\u1EDBn", L"\u01A1", 1 },
    { L"nhor", L"nh\u1ECF", L"o", 2 },
    { L"cao", L"cao", L"ao", -1 },
    { L"thaaps", L"th\u1EA5p", L"\u00E2", 2 },
    { L"daif", L"d\u00E0i", L"ai", 1 },
    { L"ngawns", L"ng\u1EAFn", L"\u0103", 2 },
    { L"nhanh", L"nhanh", L"a", -1 },
    { L"chaamj", L"ch\u1EADm", L"\u00E2", 2 },
    { L"ddepj", L"\u0111\u1EB9p", L"e", 1 },
    { L"awn", L"\u0103n", L"\u0103", -1 },
    { L"uoongs", L"u\u1ED1ng", L"u\u00F4", 1 },
    { L"ngur", L"ng\u1EE7", L"u", 2 },
    { L"ddi", L"\u0111i", L"i", -1 },
    { L"chayj", L"ch\u1EA1y", L"ay", 2 },
    { L"nois", L"n\u00F3i", L"oi", 1 },
    { L"mow", L"m\u01A1", L"\u01A1", -1 },
    { L"mowf", L"m\u1EDD", L"\u01A1", 1 },
    { L"mows", L"m\u1EDB", L"\u01A1", 1 },
    { L"mowr", L"m\u1EDF", L"\u01A1", 1 },
    { L"mowx", L"m\u1EE1", L"\u01A1", 1 },
    { L"mowj", L"m\u1EE3", L"\u01A1", 1 },
    { L"ma", L"ma", L"a", -1 },
    { L"maf", L"m\u00E0", L"a", 1 },
    { L"mas", L"m\u00E1", L"a", 1 },
    { L"mar", L"m\u1EA3", L"a", 1 },
    { L"max", L"m\u00E3", L"a", 1 },
    { L"maj", L"m\u1EA1", L"a", 1 },
    { L"huwowu", L"h\u01B0\u01A1u", L"\u01B0\u01A1u", -1 },
    { L"nguyeenx", L"nguy\u1EC5n", L"uy\u00EA", 4 },
    { L"giuwowngf", L"gi\u01B0\u1EDDng", L"\u01B0\u01A1", 3 },
    { L"truwowngf", L"tr\u01B0\u1EDDng", L"\u01B0\u01A1", 3 },
    { L"dduwowngf", L"\u0111\u01B0\u1EDDng", L"\u01B0\u01A1", 2 },
    { L"muwownj", L"m\u01B0\u1EE3n", L"\u01B0\u01A1", 2 },
    { L"xuwowngs", L"x\u01B0\u1EDBng", L"\u01B0\u01A1", 2 },
    { L"tuwowngj", L"t\u01B0\u1EE3ng", L"\u01B0\u01A1", 2 },
    { L"phuwowng", L"ph\u01B0\u01A1ng", L"\u01B0\u01A1", -1 },
    { L"thuwowng", L"th\u01B0\u01A1ng", L"\u01B0\u01A1", -1 },
    { L"khuyeets", L"khuy\u1EBFt", L"uy\u00EA", 4 },
    { L"tuyeetj", L"tuy\u1EC7t", L"uy\u00EA", 3 },
    { L"nguyeen", L"nguy\u00EAn", L"uy\u00EA", -1 },
    { L"chuyeenf", L"chuy\u1EC1n", L"uy\u00EA", 4 },
    { L"quys", L"qu\u00FD", L"y", 2 },
    { L"quaf", L"qu\u00E0", L"a", 2 },
    { L"quaay", L"qu\u00E2y", L"\u00E2y", -1 },
    { L"khuya", L"khuya", L"uya", -1 },
    { L"thuyf", L"thu\u1EF3", L"uy", 3 },
    { L"quoocs", L"qu\u1ED1c", L"u\u00F4", 2 },
    { L"yeeu", L"y\u00EAu", L"y\u00EAu", -1 },
    { L"kieemf", L"ki\u1EC1m", L"i\u00EA", 2 },
    { L"tieenf", L"ti\u1EC1n", L"i\u00EA", 2 },
    { L"chieens", L"chi\u1EBFn", L"i\u00EA", 3 },
    { L"dieenj", L"di\u1EC7n", L"i\u00EA", 2 },
    { L"hieenj", L"hi\u1EC7n", L"i\u00EA", 2 },
    { L"tieengs", L"ti\u1EBFng", L"i\u00EA", 2 },
    { L"bieengs", L"bi\u1EBFng", L"i\u00EA", 2 },
    { L"yeens", L"y\u1EBFn", L"y\u00EA", 1 },
    { L"trieeuf", L"tri\u1EC1u", L"i\u00EAu", 3 },
    { L"ddoongs", L"\u0111\u1ED1ng", L"\u00F4", 1 },
    { L"dduwa", L"\u0111\u01B0a", L"\u01B0a", -1 },
    { L"ddaif", L"\u0111\u00E0i", L"ai", 1 },
    { L"dduwngs", L"\u0111\u1EE9ng", L"\u01B0", 1 },
    { L"ddinhr", L"\u0111\u1EC9nh", L"i", 1 },
    { L"ddangr", L"\u0111\u1EA3ng", L"a", 1 },
    { L"ddowij", L"\u0111\u1EE3i", L"\u01A1i", 1 },
    { L"ddoanf", L"\u0111o\u00E0n", L"oa", 2 },
    { L"caf", L"c\u00E0", L"a", 1 },
    { L"car", L"c\u1EA3", L"a", 1 },
    { L"caj", L"c\u1EA1", L"a", 1 },
    { L"cax", L"c\u00E3", L"a", 1 },
    { L"eef", L"\u1EC1", L"\u00EA", 0 },
    { L"oof", L"\u1ED3", L"\u00F4", 0 },
    { L"os", L"\u00F3", L"o", 0 },
    { L"us", L"\u00FA", L"u", 0 },
    { L"yf", L"\u1EF3", L"y", 0 },
    { L"tuaans", L"tu\u1EA5n", L"u\u00E2", 2 },
    { L"ddieenj", L"\u0111i\u1EC7n", L"i\u00EA", 2 },
    { L"hoanf", L"ho\u00E0n", L"oa", 2 },
    { L"toans", L"to\u00E1n", L"oa", 2 },
    { L"xoans", L"xo\u00E1n", L"oa", 2 },
    { L"luaanj", L"lu\u1EADn", L"u\u00E2", 2 },
    { L"muoonj", L"mu\u1ED9n", L"u\u00F4", 2 },
    { L"buoocj", L"bu\u1ED9c", L"u\u00F4", 2 },
    { L"nguoonf", L"ngu\u1ED3n", L"u\u00F4", 3 },
    { L"vuwownj", L"v\u01B0\u1EE3n", L"\u01B0\u01A1", 2 },
    { L"hoaf", L"ho\u00E0", L"oa", 2 },
    { L"oef", L"o\u00E8", L"oe", 1 },
    { L"thuys", L"thu\u00FD", L"uy", 3 },
    { L"quees", L"qu\u1EBF", L"\u00EA", 2 },
    { L"vieex", L"vi\u1EC5", L"i\u00EA", 2 },
    { L"hoej", L"ho\u1EB9", L"oe", 2 },
    { L"ueer", L"u\u1EC3", L"u\u00EA", 1 },
    { L"quyf", L"qu\u1EF3", L"y", 2 },
    { L"rooif", L"r\u1ED3i", L"\u00F4i", 1 },
    { L"ddooif", L"\u0111\u1ED3i", L"\u00F4i", 1 },
    { L"bowi", L"b\u01A1i", L"\u01A1i", -1 },
    { L"muas", L"m\u00FAa", L"ua", 1 },
    { L"muaf", L"m\u00F9a", L"ua", 1 },
    { L"baif", L"b\u00E0i", L"ai", 1 },
    { L"tuis", L"t\u00FAi", L"ui", 1 },
    { L"tuyeens", L"tuy\u1EBFn", L"uy\u00EA", 3 },
    { L"ngoawnf", L"ngo\u1EB1n", L"oa", 3 },
    { L"ngoaif", L"ngo\u00E0i", L"oai", 3 },
    { L"nguwowif", L"ng\u01B0\u1EDDi", L"\u01B0\u01A1i", 3 },
    { L"ruwowij", L"r\u01B0\u1EE3i", L"\u01B0\u01A1i", 2 },
    { L"tuooir", L"tu\u1ED5i", L"u\u00F4", 2 },
    { L"giaf", L"gi\u00E0", L"a", 2 },
    { L"giaos", L"gi\u00E1o", L"ao", 2 },
    { L"giar", L"gi\u1EA3", L"a", 2 },
    { L"giups", L"gi\u00FAp", L"u", 2 },
    { L"ddeem", L"\u0111\u00EAm", L"\u00EA", -1 },
    { L"sangs", L"s\u00E1ng", L"a", 1 },
    { L"chieeuf", L"chi\u1EC1u", L"i\u00EA", 3 },
    { L"truwa", L"tr\u01B0a", L"\u01B0a", -1 },
    { L"hoom", L"h\u00F4m", L"\u00F4", -1 },
    { L"nay", L"nay", L"ay", -1 },
};

static constexpr int g_vietnameseCorpusCount = 200;

// ---------------------------------------------------------------------------
// 50 từ tiếng Anh — coverage cho Requirement 8 bypass rules:
//   - 10 từ bắt đầu w/f/j (R8.1)
//   - 5  từ q + non-u    (R8.2)
//   - 10 từ p + non-h    (R8.3)
//   - 15 từ cluster đầu không Việt (R8.4)
//   - 10 từ z/w/structural khác
// ---------------------------------------------------------------------------
static const EnglishEntry g_englishCorpus[50] = {
    { "write" },
    { "work" },
    { "world" },
    { "want" },
    { "fast" },
    { "food" },
    { "free" },
    { "flag" },
    { "just" },
    { "jump" },
    { "qmail" },
    { "qatar" },
    { "qed" },
    { "qrs" },
    { "qwerty" },
    { "public" },
    { "project" },
    { "plan" },
    { "place" },
    { "post" },
    { "pro" },
    { "pop" },
    { "pin" },
    { "pay" },
    { "pen" },
    { "class" },
    { "style" },
    { "block" },
    { "brick" },
    { "drink" },
    { "smell" },
    { "stack" },
    { "swing" },
    { "sleep" },
    { "click" },
    { "dream" },
    { "cry" },
    { "fly" },
    { "glad" },
    { "scan" },
    { "zoom" },
    { "zero" },
    { "will" },
    { "when" },
    { "width" },
    { "height" },
    { "function" },
    { "import" },
    { "output" },
    { "input" },
};

static constexpr int g_englishCorpusCount = 50;

} // namespace CayTestCorpus

#endif // CAY_TEST_CORPUS_H

