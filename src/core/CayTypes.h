#pragma once
// ============================================================================
// CayTypes.h — Kiểu dữ liệu cơ bản & hằng số dùng chung của Engine_Core
//
// Nội dung:
//   - constexpr Cay::MAX_BUFFER = 64    (single source — không redefine)
//   - enum class KeyCode                (mã phím Unknown / Backspace / A..Z ...)
//   - struct KeyEvent                   (keyCode + character + handled)
//   - typedef InjectTextFunc            (callback platform inject text)
//   - inline helper CayStrLen / CayStrCmp (thay thế CRT wcslen / wcscmp)
//
// Ràng buộc:
//   * No-CRT      — không include <cstring>, <cwchar>, không gọi wcslen/memcpy.
//   * No-STL      — chỉ <cstdint> cho uint32_t.
//   * No-alloc    — toàn bộ là POD / constexpr / inline static linkage.
//   * No-except   — không throw, không try/catch.
// ============================================================================
#include <cstdint>

namespace Cay {

    // Kích thước cố định cho buffer phím thô (`_buffer`) và buffer text output
    // (`_text`) trong `TelexEngine`. Cũng được platform injector dùng để khai
    // báo array tạm. Single source of truth — KHÔNG khai báo lại bằng `#define`
    // hoặc literal `64` ở nơi khác.
    constexpr int MAX_BUFFER = 64;

    // Mã phím - enum các phím được hỗ trợ
    enum class KeyCode : uint32_t {
        Unknown = 0,
        Backspace, Escape, Enter, Tab, Space,
        Left, Right, Up, Down, Home, End, PageUp, PageDown, Delete,
        KeyA = 'A', KeyB = 'B', KeyC = 'C', KeyD = 'D', KeyE = 'E',
        KeyF = 'F', KeyG = 'G', KeyH = 'H', KeyI = 'I', KeyJ = 'J',
        KeyK = 'K', KeyL = 'L', KeyM = 'M', KeyN = 'N', KeyO = 'O',
        KeyP = 'P', KeyQ = 'Q', KeyR = 'R', KeyS = 'S', KeyT = 'T',
        KeyU = 'U', KeyV = 'V', KeyW = 'W', KeyX = 'X', KeyY = 'Y', KeyZ = 'Z'
    };

    // Sự kiện phím - chứa thông tin về phím được nhấn
    struct KeyEvent {
        KeyCode keyCode;          // Mã phím
        wchar_t character;        // Ký tự Unicode đã chuyển đổi
        bool handled;             // Đã xử lý hay chưa
    };

    // Hàm callback để inject text - được gọi khi cần thay thế text
    typedef void (*InjectTextFunc)(int backspaceCount, const wchar_t* newText, int newTextLen);

    // Helper: tính độ dài chuỗi wide-char (không dùng CRT)
    inline int CayStrLen(const wchar_t* s) {
        int len = 0;
        while (s && *s) { len++; s++; }
        return len;
    }
    // Helper: so sánh chuỗi wide-char (không dùng CRT)
    inline int CayStrCmp(const wchar_t* s1, const wchar_t* s2) {
        while (*s1 && (*s1 == *s2)) { s1++; s2++; }
        return *s1 - *s2;
    }
} // namespace Cay

