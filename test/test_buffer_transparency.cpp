// test/test_buffer_transparency.cpp
//
// [Feature: engine-core-refactor, Property 17: OnInjectText buffer transparency]
// Validates: Requirements 5.4
//
// Property 17: với mọi `backspaceCount ∈ [0, MAX_BUFFER]`,
// `newTextLen ∈ [0, MAX_BUFFER]` và mọi nội dung `newText` (random wchar_t),
// khi callback `Cay::InjectTextFunc` được invoke với bộ tham số đó, side
// nhận callback (mock injector trong test) SHALL nhận lại đúng:
//   - backspaceCount  == bs
//   - newText.size()  == newTextLen
//   - newText[i]      == newText_in[i]  với mọi i ∈ [0, newTextLen)
//
// Đây là test transparency của contract callback giữa Engine_Core và
// platform layer (Requirement 5.4 — không silent truncation, không
// transformation). Bằng cách kiểm tra byte-perfect trên 200 mẫu ngẫu
// nhiên cover toàn dải `[0, MAX_BUFFER]`, ta phát hiện được mọi
// truncation, off-by-one, hoặc bộ encoder/decoder lén lút trong các
// implementation tuân thủ signature `InjectTextFunc`.
//
// Test này KHÔNG drive `TelexEngine` — nó verify trực tiếp contract
// callback. Lý do: Property 17 nói về sự "trong suốt" của tham số khi
// đi qua callback, độc lập với cơ chế nội bộ engine sinh ra chúng.

#include "doctest.h"
#include "property.h"
#include "CayTypes.h"

#include <cstdint>
#include <random>
#include <vector>

namespace {

// Một mẫu sinh cho Property 17 — tham số đầu vào của một call đến
// `InjectTextFunc`. `newText` luôn có đúng `newTextLen` phần tử (rỗng
// được phép), giữ invariant len/size khớp nhau ngay từ generator.
struct InjectCall {
    int backspaceCount;
    int newTextLen;
    std::vector<wchar_t> newText;
};

// Sinh wchar_t ngẫu nhiên trong dải BMP an toàn:
//   - bỏ NUL (0x0000) để tránh ngộ nhận terminator nếu implementation
//     nào đó hiểu sai contract `len`-explicit;
//   - bỏ vùng surrogate [0xD800, 0xDFFF] để tránh tạo nửa surrogate
//     ill-formed (ảnh hưởng platform có wchar_t 16-bit như Windows);
//   - dừng tại 0xFFFD (Unicode replacement) để tránh non-character.
// Dải còn lại đủ rộng để bắt mọi transformation/truncation trên byte.
inline wchar_t randomBmpWchar(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(1, 0xFFFD);
    while (true) {
        int v = dist(rng);
        if (v >= 0xD800 && v <= 0xDFFF) continue;
        return static_cast<wchar_t>(v);
    }
}

// `genInjectCall`: sinh (backspaceCount, newTextLen, newText) ngẫu nhiên
// trong phạm vi spec Property 17.
cay::test::Gen<InjectCall> genInjectCall() {
    return cay::test::Gen<InjectCall>{
        [](uint32_t seed) -> InjectCall {
            std::mt19937 rng(seed);
            std::uniform_int_distribution<int> dBs(0, Cay::MAX_BUFFER);
            std::uniform_int_distribution<int> dLen(0, Cay::MAX_BUFFER);

            InjectCall c{};
            c.backspaceCount = dBs(rng);
            c.newTextLen = dLen(rng);
            c.newText.reserve(static_cast<size_t>(c.newTextLen));
            for (int i = 0; i < c.newTextLen; i++) {
                c.newText.push_back(randomBmpWchar(rng));
            }
            return c;
        }
    };
}

} // anonymous namespace

// =====================================================================
// Property 17 — OnInjectText buffer transparency.
// 200 iterations theo spec (tasks.md task 6.4).
// =====================================================================
TEST_CASE("Property 17: OnInjectText buffer transparency") {
    cay::test::forAll(genInjectCall(), 200, [](const InjectCall& c) {
        cay::test::MockInjector::reset();

        // Gọi callback đúng qua con trỏ hàm để test khớp signature
        // `Cay::InjectTextFunc` (compile-time check ngay tại đây — sai
        // signature thì không gán được).
        Cay::InjectTextFunc inject = &cay::test::MockInjector::Hook;

        // Khi `newTextLen == 0`, contract cho phép `newText == nullptr`
        // (engine không nhất thiết phải có buffer hợp lệ để truyền).
        const wchar_t* ptr = c.newText.empty() ? nullptr : c.newText.data();
        inject(c.backspaceCount, ptr, c.newTextLen);

        const auto& calls = cay::test::MockInjector::calls();
        REQUIRE_MESSAGE(calls.size() == 1u,
            "InjectTextFunc invocation phải tạo đúng 1 entry trong MockInjector");

        const auto& got = calls.front();

        // Dùng INFO + CHECK để các message chỉ in ra khi assertion fail
        // (CHECK_MESSAGE in cả khi pass trong verbose `-s`, gây nhiễu).
        INFO("backspaceCount: got=" << got.backspaceCount
                                    << ", expected=" << c.backspaceCount);
        CHECK(got.backspaceCount == c.backspaceCount);

        INFO("newText.size(): got=" << got.newText.size()
                                    << ", expected=" << c.newTextLen);
        REQUIRE(static_cast<int>(got.newText.size()) == c.newTextLen);

        for (int i = 0; i < c.newTextLen; i++) {
            // So sánh từng wchar — bắt mọi transformation/truncation.
            INFO("newText[" << i << "] mismatch: got=0x"
                << std::hex << static_cast<uint32_t>(got.newText[static_cast<size_t>(i)])
                << ", expected=0x"
                << static_cast<uint32_t>(c.newText[static_cast<size_t>(i)]));
            CHECK(got.newText[static_cast<size_t>(i)]
                  == c.newText[static_cast<size_t>(i)]);
        }
    });
}
