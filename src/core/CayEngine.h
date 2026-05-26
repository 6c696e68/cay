#pragma once
// ============================================================================
// CayEngine.h — TelexEngine state machine (xử lý chính của Cay)
//
// Sở hữu state (đều là array tĩnh kích thước Cay::MAX_BUFFER từ CayTypes.h):
//   - _buffer[MAX_BUFFER]  : phím thô user gõ (MyKey)
//   - _text  [MAX_BUFFER]  : ký tự output hiện tại (đã transform)
//   - _toneIndex / _saved* : metadata commit & recall từ
//
// Tất cả modifier (double, hook, tone) áp bằng backward-scan trên _text
// (QUY TẮC 3). Mọi bảng âm tiết được tham chiếu qua API CayData — engine
// KHÔNG redefine s_initials/s_nuclei/s_finals/s_tails.
//
// API chính: TelexEngine::OnKeyDown / OnKeyUp / ResetFull / CommitWord;
// callback Cay::InjectTextFunc OnInjectText = nullptr (do platform set).
//
// Ràng buộc:
//   * No-CRT / No-STL / No-alloc / No-except — phù hợp Windows no-CRT build.
//   * Dùng Cay::MAX_BUFFER (CayTypes.h) — không hardcode 64 trong file này.
// ============================================================================
#include "CayData.h"
#include "CayTypes.h"

namespace Cay {

// ---------------------------------------------------------------------------
// MyKey – bản ghi compact của một phím thô + ký tự nó tạo ra
// trong output buffer (có thể khác với phím thô sau khi transform).
// ---------------------------------------------------------------------------
struct MyKey {
    wchar_t raw;       // Ký tự thô người dùng gõ (ví dụ 'a', 'w', 's')
    wchar_t output;    // Ký tự hiện tại trong output text
};

#ifdef CAY_TEST_BUILD
// ---------------------------------------------------------------------------
// DebugState (test-only) — snapshot toàn bộ field nội bộ của TelexEngine
// để Property 11 / 12 (test/test_idempotency.cpp) so sánh state.
//
// Layout phản chiếu đúng các field private của TelexEngine. Các array dùng
// kích thước Cay::MAX_BUFFER cố định nên struct là POD; test code có thể
// so sánh field-by-field hoặc memcmp tuỳ ý (free operator== trong test file).
//
// Validates: Requirements 10.1, 10.2, 10.3
// ---------------------------------------------------------------------------
struct DebugState {
    MyKey   buffer[MAX_BUFFER];
    int     bufferCount;
    wchar_t text[MAX_BUFFER];
    int     textLen;
    int     toneIndex;
    wchar_t lastOutput[MAX_BUFFER];
    int     lastOutputLen;
    MyKey   savedBuffer[MAX_BUFFER];
    int     savedBufferCount;
    wchar_t savedText[MAX_BUFFER];
    int     savedTextLen;
    int     savedToneIndex;
    bool    canRestore;
};
#endif

// ---------------------------------------------------------------------------
// TelexEngine
//
// State machine xử lý Telex chính.
// Sở hữu:
//   _buffer[MAX_BUFFER]  – bản ghi phím thô
//   _bufferCount         – số entry hợp lệ trong _buffer
//   _text[MAX_BUFFER]    – ký tự output hiện tại (đã được inject)
//   _textLen             – độ dài của _text
//
// Tất cả xử lý được thực hiện qua backward-scan (QUY TẮC 3).
// Không cấp phát động, không STL, không CRT.
// ---------------------------------------------------------------------------
class TelexEngine {
public:
    TelexEngine();

    // Được gọi mỗi khi keydown (main.cpp delegate đến đây).
    void OnKeyDown(Cay::KeyEvent& e);

    // Required by platform contract (Windows KeyboardHookManager,
    // MacHookManager forward keyup events). Currently no-op.
    void OnKeyUp(Cay::KeyEvent& e);

    // Hard reset: flush buffer và discard tất cả state.
    void ResetFull();

    // Commit từ hiện tại: lưu state để recall, sau đó reset.
    void CommitWord();

    InjectTextFunc OnInjectText = nullptr;

#ifdef CAY_TEST_BUILD
    // -----------------------------------------------------------------------
    // Test-only accessors — chỉ tồn tại khi build với BUILD_TESTING=ON
    // (target `cay_test`). Release binary KHÔNG chứa các symbol này.
    //
    // Mục đích: cho phép property-based tests truy cập các quan sát nội bộ
    // (private const helpers, internal _text buffer) mà không phá vỡ
    // encapsulation của release API.
    // -----------------------------------------------------------------------
    bool DebugShouldBypassWord() const { return ShouldBypassWord(); }

    // Cho phép test set _text trực tiếp để cô lập FindTonePosition khỏi
    // chuỗi keystroke (Property 9). Truncate nếu len > MAX_BUFFER - 1.
    void DebugSetText(const wchar_t* s, int len);

    // Wrapper test-only cho FindTonePosition (private const).
    int  DebugFindTonePosition() const { return FindTonePosition(); }

    // Đọc nội dung _text[] hiện tại (read-only). Không null-terminate buffer
    // được trả về — caller phải dùng kèm DebugTextLen().
    const wchar_t* DebugText()    const { return _text; }
    int            DebugTextLen() const { return _textLen; }

    // Test-only wrappers cho StripAllTones() và ResetState() (private).
    // Property 11 (test/test_idempotency.cpp) cần gọi trực tiếp 2 reset
    // operations để kiểm tra idempotency mà không phải mô phỏng qua
    // chuỗi phím (Space/Backspace có thể gây side-effects khác).
    void DebugStripAllTones() { StripAllTones(); }
    void DebugResetState()    { ResetState(); }

    // Snapshot toàn bộ state nội bộ vào DebugState (POD copy).
    // Dùng cho Property 11 / 12 để so sánh state trước-sau reset operation.
    DebugState GetDebugState() const;
#endif

private:
    MyKey _buffer[MAX_BUFFER];
    int   _bufferCount;

    wchar_t _text[MAX_BUFFER];
    int     _textLen;

    // Chỉ số dấu hiện tại (0–5) được áp dụng cho từ này, hoặc -1 nếu không có.
    int  _toneIndex;

    // Tracking output để update màn hình chính xác (tính toán diff chính xác)
    wchar_t _lastOutput[MAX_BUFFER];
    int     _lastOutputLen;

    // State recall từ
    MyKey   _savedBuffer[MAX_BUFFER];
    int     _savedBufferCount;
    wchar_t _savedText[MAX_BUFFER];
    int     _savedTextLen;
    int     _savedToneIndex;
    bool    _canRestore;

    void SaveState();

    // -----------------------------------------------------------------------
    // Các bước transform chính – tất cả mutate _text[] in place.
    // -----------------------------------------------------------------------

    // Xử lý phím dấu mũ đôi (aa->â, ee->ê, oo->ô, dd->đ).
    // Trả về true nếu phím được tiêu thụ như modifier phím đôi.
    bool ApplyDoubleKeys(wchar_t key);

    // Xử lý phím dấu mũ/dấu ngắn ('w').
    // Trả về true nếu phím được tiêu thụ như modifier dấu mũ.
    bool ApplyHookKeys(wchar_t key);

    // Áp dụng (hoặc thay đổi) dấu thanh vào output buffer.
    // Trả về true nếu dấu được áp dụng.
    bool ApplyToneMarks(int toneIndex);

    // Bỏ tất cả dấu thanh từ _text[], rewrite in place.
    void StripAllTones();

    // Tìm index nguyên âm ngoài cùng bên phải trong _text[0.._textLen)
    // là ứng viên tốt để nhận dấu thanh (quy tắc đặt dấu tiếng Việt).
    int FindTonePosition() const;

    // Update màn hình hiệu quả bằng cách tính toán backspaces chính xác
    void UpdateScreen(const wchar_t* newOutput, int newOutputLen);

    // Commit từ hiện tại: inject _text[] để thay thế những gì user thấy.
    void Commit(int extraBs = 0);

    // Revert về input ASCII thô (fallback tiếng Anh).
    void FallbackToRaw();

    // Kiểm tra xem buffer hiện tại có trông giống từ tiếng Anh không
    // và nên bypass xử lý tiếng Việt.
    bool ShouldBypassWord() const;

    // Reset internal state mà không gửi input nào.
    void ResetState();

    // Replay 1 phím thô vào engine (record + try modifiers + fallback append).
    // Dùng chung cho cả OnKeyDown lẫn Backspace replay, tránh duplicate logic.
    void ReplayKey(wchar_t ch);

    // Helpers
    static bool IsAlpha(wchar_t ch);
};

#ifdef CAY_TEST_BUILD
// ---------------------------------------------------------------------------
// Test-only accessor cho file-scope helper `IsCompleteSyllable` định nghĩa
// trong `CayEngine.cpp`. Release build giữ nguyên `static` linkage để compiler
// inline + strip; test build (`CAY_TEST_BUILD`) bỏ `static` để Property 13
// (`test/test_syllable_round_trip.cpp`) có thể gọi trực tiếp.
//
// Validates: Requirements 17.2, 17.3, 17.4
// ---------------------------------------------------------------------------
bool IsCompleteSyllable(const wchar_t* s, int len);
#endif

} // namespace Cay
