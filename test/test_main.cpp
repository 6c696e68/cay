// =====================================================================
// test/test_main.cpp — Entry point cho cay_test executable.
//
// Trước Task 1.5: dùng DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN (auto-main).
// Sau Task 1.5: dùng DOCTEST_CONFIG_IMPLEMENT + custom main() để hỗ trợ
//   ./cay_test --record-golden  → ghi test/baseline_traces.csv (chế độ
//                                  ghi nhận trace, không chạy doctest).
//   ./cay_test                  → chạy toàn bộ TEST_CASE (gồm P15 verify
//                                  diff trace với baseline).
//
// recordGoldenTrace() được định nghĩa trong test/golden_trace.cpp.
// =====================================================================

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <cstring>

// Forward declaration — implementation ở test/golden_trace.cpp.
// Trả 0 nếu ghi baseline thành công, !=0 nếu lỗi I/O.
int recordGoldenTrace(const char* outputPath);

int main(int argc, char** argv) {
    // Phát hiện --record-golden sớm, không truyền cho doctest.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--record-golden") == 0) {
            // outputPath = nullptr → golden_trace.cpp tự resolve mặc định
            // (test/baseline_traces.csv hoặc ../test/baseline_traces.csv).
            return recordGoldenTrace(nullptr);
        }
    }
    doctest::Context ctx(argc, argv);
    return ctx.run();
}
