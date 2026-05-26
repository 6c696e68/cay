// =====================================================================
// test/golden_trace.cpp — P15: Golden trace record + check.
//
// Nhiệm vụ Task 1.5 (engine-core-refactor):
//   1) recordGoldenTrace(path):
//      - Sinh 1000 chuỗi KeyEvent (độ dài 1..32) bằng genKeySeq(32),
//        seed = i ^ 42 với i ∈ [0, 999] (parent seed 42 theo spec).
//      - Replay từng chuỗi qua TelexEngine, hook MockInjector, capture
//        trace = vector<MockCall>.
//      - Ghi mỗi dòng `seed,key_sequence_hex,trace_hex` vào file CSV.
//      - Được gọi từ main() khi cay_test chạy với --record-golden.
//
//   2) TEST_CASE("P15: golden trace ..."):
//      - Load test/baseline_traces.csv (nếu chưa có thì SKIP qua MESSAGE).
//      - Mỗi dòng: parse seed → regen key_sequence từ seed → replay engine
//        → tính trace_hex hiện tại → REQUIRE(trace_hex == stored).
//
// Format encoding:
//   key_sequence_hex: với mỗi KeyEvent, ghi 8 hex chars =
//       keyCode  (uint32 → 8 hex)? KHÔNG: dùng 4 hex cho keyCode (đủ vì
//       enum ≤ 0x005A = 'Z'), + 4 hex cho character (wchar_t & 0xFFFF).
//       Tổng: 8 hex/event.
//   trace_hex: mỗi MockCall = `bs(4hex)len(4hex)[char(4hex)]*`,
//       các call cách nhau bằng ';'. Trace rỗng → chuỗi rỗng.
//   key_sequence_hex và trace_hex chỉ chứa [0-9a-f;] → an toàn CSV
//       (không cần escape).
//
// Path convention: chạy cay_test từ repo root → "test/baseline_traces.csv".
// Nếu chạy từ build/ → fallback "../test/baseline_traces.csv".
// =====================================================================

#include "doctest.h"
#include "property.h"

#include "CayEngine.h"
#include "CayTypes.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------
// Hằng số.
// ---------------------------------------------------------------------
constexpr int   kGoldenIterations = 1000;
constexpr int   kGoldenMaxKeyLen  = 32;
constexpr uint32_t kGoldenParentSeed = 42u;
constexpr const char* kDefaultBaselinePath  = "test/baseline_traces.csv";
constexpr const char* kFallbackBaselinePath = "../test/baseline_traces.csv";

// ---------------------------------------------------------------------
// Hex encoding helpers (lower-case, fixed width).
// ---------------------------------------------------------------------
inline char nibbleToHex(unsigned v) {
    v &= 0xFu;
    return static_cast<char>(v < 10 ? '0' + v : 'a' + (v - 10));
}

inline void appendHex16(std::string& out, uint16_t v) {
    out.push_back(nibbleToHex((v >> 12) & 0xF));
    out.push_back(nibbleToHex((v >> 8)  & 0xF));
    out.push_back(nibbleToHex((v >> 4)  & 0xF));
    out.push_back(nibbleToHex(v         & 0xF));
}

// ---------------------------------------------------------------------
// Encode chuỗi KeyEvent → hex string.
// Mỗi event: 4 hex (keyCode & 0xFFFF) + 4 hex (character & 0xFFFF) = 8 hex.
// ---------------------------------------------------------------------
std::string keySeqToHex(const std::vector<Cay::KeyEvent>& seq) {
    std::string out;
    out.reserve(seq.size() * 8);
    for (const auto& e : seq) {
        appendHex16(out, static_cast<uint16_t>(static_cast<uint32_t>(e.keyCode) & 0xFFFFu));
        appendHex16(out, static_cast<uint16_t>(static_cast<uint32_t>(e.character) & 0xFFFFu));
    }
    return out;
}

// ---------------------------------------------------------------------
// Encode trace (vector<MockCall>) → hex string.
// Mỗi call: bs(4hex) + len(4hex) + len ký tự × 4hex. Các call ngăn cách
// bằng ';'. Trace rỗng → chuỗi rỗng.
// ---------------------------------------------------------------------
std::string traceToHex(const std::vector<cay::test::MockCall>& calls) {
    std::string out;
    bool first = true;
    for (const auto& c : calls) {
        if (!first) out.push_back(';');
        first = false;
        // backspaceCount có thể âm (không nên xảy ra) → cast về uint16
        // để không phình size CSV; engine không phát hành bs > 65535.
        appendHex16(out, static_cast<uint16_t>(c.backspaceCount & 0xFFFF));
        appendHex16(out, static_cast<uint16_t>(c.newText.size() & 0xFFFF));
        for (wchar_t wc : c.newText) {
            appendHex16(out, static_cast<uint16_t>(static_cast<uint32_t>(wc) & 0xFFFFu));
        }
    }
    return out;
}

// ---------------------------------------------------------------------
// Replay 1 chuỗi KeyEvent qua TelexEngine, return trace.
// Engine mới được tạo mỗi lần → state hoàn toàn fresh.
// ---------------------------------------------------------------------
std::vector<cay::test::MockCall> runEngineCapture(const std::vector<Cay::KeyEvent>& seq) {
    Cay::TelexEngine engine;
    engine.OnInjectText = &cay::test::MockInjector::Hook;
    cay::test::MockInjector::reset();
    for (Cay::KeyEvent e : seq) {
        engine.OnKeyDown(e);
    }
    // Copy ra trước khi reset() lần kế tiếp ghi đè.
    return cay::test::MockInjector::calls();
}

// ---------------------------------------------------------------------
// Sinh seed con cho iteration thứ i. XOR với parent seed để giữ tính
// "có liên hệ với parent seed 42" theo spec, vẫn deterministic theo i.
// ---------------------------------------------------------------------
inline uint32_t childSeedFor(int i) {
    return static_cast<uint32_t>(i) ^ kGoldenParentSeed;
}

// ---------------------------------------------------------------------
// Mở baseline file để đọc: thử kDefaultBaselinePath trước, fallback sang
// kFallbackBaselinePath. Trả tên path mở thành công qua outResolvedPath
// (nếu non-null). Nếu không file nào tồn tại, ifstream trả !is_open().
// ---------------------------------------------------------------------
std::ifstream openBaselineForRead(std::string* outResolvedPath = nullptr) {
    std::ifstream f(kDefaultBaselinePath);
    if (f.is_open()) {
        if (outResolvedPath) *outResolvedPath = kDefaultBaselinePath;
        return f;
    }
    f.clear();
    f.open(kFallbackBaselinePath);
    if (f.is_open() && outResolvedPath) *outResolvedPath = kFallbackBaselinePath;
    return f;
}

// ---------------------------------------------------------------------
// Resolve path để ghi: nếu path != nullptr → dùng nguyên. Nếu nullptr,
// thử ghi vào kDefaultBaselinePath; nếu fail (CWD không phải repo root)
// rớt sang kFallbackBaselinePath.
// ---------------------------------------------------------------------
std::ofstream openBaselineForWrite(const char* path, std::string* outResolvedPath) {
    if (path != nullptr) {
        std::ofstream f(path);
        if (f.is_open() && outResolvedPath) *outResolvedPath = path;
        return f;
    }
    std::ofstream f(kDefaultBaselinePath);
    if (f.is_open()) {
        if (outResolvedPath) *outResolvedPath = kDefaultBaselinePath;
        return f;
    }
    f.clear();
    f.open(kFallbackBaselinePath);
    if (f.is_open() && outResolvedPath) *outResolvedPath = kFallbackBaselinePath;
    return f;
}

// ---------------------------------------------------------------------
// Parse 1 dòng CSV "seed,keyseq_hex,trace_hex" (3 trường, không escape).
// Trả false nếu format sai.
// ---------------------------------------------------------------------
bool parseCsvLine(const std::string& line,
                  uint32_t& outSeed,
                  std::string& outKeySeqHex,
                  std::string& outTraceHex) {
    // Bỏ qua dòng rỗng / chỉ whitespace.
    if (line.empty()) return false;

    size_t c1 = line.find(',');
    if (c1 == std::string::npos) return false;
    size_t c2 = line.find(',', c1 + 1);
    if (c2 == std::string::npos) return false;
    // Đảm bảo không có dấu phẩy thứ 3 (trace_hex chỉ chứa 0-9a-f và ';').
    if (line.find(',', c2 + 1) != std::string::npos) return false;

    const std::string seedStr = line.substr(0, c1);
    if (seedStr.empty()) return false;
    try {
        outSeed = static_cast<uint32_t>(std::stoul(seedStr));
    } catch (...) {
        return false;
    }
    outKeySeqHex = line.substr(c1 + 1, c2 - c1 - 1);
    outTraceHex  = line.substr(c2 + 1);
    return true;
}

} // anonymous namespace

// =====================================================================
// recordGoldenTrace — public entry point gọi từ test_main.cpp khi nhận
// arg --record-golden.
// =====================================================================
int recordGoldenTrace(const char* outputPath) {
    std::string resolved;
    std::ofstream out = openBaselineForWrite(outputPath, &resolved);
    if (!out.is_open()) {
        std::fprintf(stderr,
                     "[golden_trace] FATAL: không mở được baseline để ghi "
                     "(thử %s và %s). CWD có đúng là repo root?\n",
                     kDefaultBaselinePath, kFallbackBaselinePath);
        return 1;
    }

    auto gen = cay::test::genKeySeq(kGoldenMaxKeyLen);
    int written = 0;
    for (int i = 0; i < kGoldenIterations; ++i) {
        const uint32_t seed = childSeedFor(i);
        const std::vector<Cay::KeyEvent> seq = gen.sample(seed);
        const std::string keyHex = keySeqToHex(seq);
        const std::vector<cay::test::MockCall> trace = runEngineCapture(seq);
        const std::string traceHex = traceToHex(trace);

        out << seed << ',' << keyHex << ',' << traceHex << '\n';
        if (!out.good()) {
            std::fprintf(stderr,
                         "[golden_trace] FATAL: lỗi I/O khi ghi dòng %d vào %s\n",
                         i, resolved.c_str());
            return 2;
        }
        ++written;
    }
    out.flush();
    std::fprintf(stdout,
                 "[golden_trace] Đã ghi %d dòng vào %s\n",
                 written, resolved.c_str());
    return 0;
}

// =====================================================================
// P15 TEST_CASE — verify trace khớp baseline.
// Validates: Requirements 1.1, 1.2, 1.3, 7.8, 8.7, 18.3
// =====================================================================
TEST_CASE("P15: golden trace match baseline") {
    std::string resolved;
    std::ifstream in = openBaselineForRead(&resolved);
    if (!in.is_open()) {
        MESSAGE("Skipped: chưa có baseline "
                << std::string(kDefaultBaselinePath)
                << " — chạy `cay_test --record-golden` (từ repo root) trước.");
        return;
    }

    auto gen = cay::test::genKeySeq(kGoldenMaxKeyLen);
    int lineNo = 0;
    int verified = 0;
    std::string line;

    while (std::getline(in, line)) {
        ++lineNo;
        if (line.empty()) continue;

        uint32_t seed = 0;
        std::string storedKeyHex;
        std::string storedTraceHex;
        if (!parseCsvLine(line, seed, storedKeyHex, storedTraceHex)) {
            FAIL("Baseline " << resolved << " dòng " << lineNo
                 << ": format không hợp lệ");
        }

        const std::vector<Cay::KeyEvent> seq = gen.sample(seed);
        const std::string currentKeyHex = keySeqToHex(seq);

        // Sanity: nếu key sequence regen không khớp baseline → generator
        // hoặc seed đã thay đổi → toàn bộ baseline mất ý nghĩa.
        REQUIRE_MESSAGE(currentKeyHex == storedKeyHex,
                        "Baseline " << resolved << " dòng " << lineNo
                        << " seed=" << seed
                        << ": key_sequence_hex regen khác baseline "
                        << "(generator/seed scheme đã thay đổi?). "
                        << "Cần record lại baseline.");

        const std::vector<cay::test::MockCall> trace = runEngineCapture(seq);
        const std::string currentTraceHex = traceToHex(trace);

        REQUIRE_MESSAGE(currentTraceHex == storedTraceHex,
                        "Baseline " << resolved << " dòng " << lineNo
                        << " seed=" << seed
                        << ": trace_hex thay đổi.\n  expected=" << storedTraceHex
                        << "\n  actual  =" << currentTraceHex);
        ++verified;
    }

    MESSAGE("P15 verified " << verified << " baseline traces từ " << resolved);
    REQUIRE(verified > 0);
}
