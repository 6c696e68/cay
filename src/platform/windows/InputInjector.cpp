#include "InputInjector.h"
#include "CayTypes.h"

// ---------------------------------------------------------------------------
// Worst-case sizing cho mảng INPUT của một lần `ReplaceText` duy nhất.
//
//   Worst case: 1 dummy pair + (MAX_BUFFER + 1) backspace pairs +
//               MAX_BUFFER unicode pairs = MAX_BUFFER*4 + 4
//
// Phân tách:
//   • 1 dummy pair        = 2 INPUT  (down + up của ký tự 'a' wake-up caret)
//   • (MAX_BUFFER + 1)
//     backspace pairs    = 2 * (MAX_BUFFER + 1) INPUT
//     (xoá dummy + tối đa MAX_BUFFER ký tự text gốc)
//   • MAX_BUFFER unicode
//     pairs              = 2 * MAX_BUFFER INPUT
//     (chèn lại text mới, không thể dài hơn MAX_BUFFER vì engine cap ở đó)
//
// Tổng = 2 + 2*(MAX_BUFFER + 1) + 2*MAX_BUFFER
//      = 2 + 2*MAX_BUFFER + 2 + 2*MAX_BUFFER
//      = 4*MAX_BUFFER + 4
//
// Với MAX_BUFFER = 64 (xem `Cay::MAX_BUFFER` trong `core/CayTypes.h`)
// ⇒ kích thước mảng = 260 INPUT structs.
// ---------------------------------------------------------------------------

namespace CayIME {

// ---------------------------------------------------------------------------
// Internal helper – điền INPUT cho cặp keydown/keyup Unicode.
// ---------------------------------------------------------------------------
static void FillUnicodeInput(INPUT* inp, wchar_t ch, DWORD flags) {
    inp->type           = INPUT_KEYBOARD;
    inp->ki.wVk         = 0;
    inp->ki.wScan       = (WORD)ch;
    inp->ki.dwFlags     = flags | KEYEVENTF_UNICODE;
    inp->ki.time        = 0;
    inp->ki.dwExtraInfo = InputInjector::MAGIC_EXTRA_INFO;
}

// ---------------------------------------------------------------------------
// Internal helper – điền INPUT cho cặp keydown/keyup virtual-key.
// ---------------------------------------------------------------------------
static void FillVkInput(INPUT* inp, WORD vk, DWORD flags) {
    inp->type           = INPUT_KEYBOARD;
    inp->ki.wVk         = vk;
    inp->ki.wScan       = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    inp->ki.dwFlags     = flags;
    inp->ki.time        = 0;
    inp->ki.dwExtraInfo = InputInjector::MAGIC_EXTRA_INFO;
}

// ---------------------------------------------------------------------------
// ReplaceText – injection batched chính.
//
// Layout của mảng INPUT duy nhất:
//   [0]      ZWJ down
//   [1]      ZWJ up
//   [2..2+2*bs-1]   bs*(VK_BACK down + VK_BACK up)
//   còn lại       newText unicode pairs
// ---------------------------------------------------------------------------
void InputInjector::ReplaceText(int backspaceCount, const wchar_t* newText, int newTextLen) {
    if (backspaceCount <= 0 && newTextLen <= 0) return;

    // Mảng tĩnh có headroom đúng worst case (xem header comment ở trên).
    INPUT inputs[Cay::MAX_BUFFER * 4 + 4];
    int idx = 0;

    // 1. ZWJ DUMMY INJECTION (Chrome/Excel Autocomplete Breaker)
    // Chúng ta chỉ inject dummy nếu thực sự đang thay thế text (backspaceCount > 0).
    // 1. Dummy character để wake up caret của target window.
    // ZWJ (\u200D) và ZWSP (\u200B) bị drop bởi các editor nghiêm ngặt như GitHub/CodeMirror.
    // Để 100% bulletproof trên tất cả editor, chúng ta dùng chữ cái printable chuẩn ('a').
    // Vì nó được insert và ngay lập tức backspaced trong cùng batch sự kiện OS,
    // nó không bao giờ flash trên màn hình và safely clears bất kỳ selection autocomplete nào.
    bool useDummy = (backspaceCount > 0);
    if (useDummy) {
        FillUnicodeInput(&inputs[idx++], L'a', 0);               // Dummy down
        FillUnicodeInput(&inputs[idx++], L'a', KEYEVENTF_KEYUP); // Dummy up
    }

    // 2. DELETION (Xóa dummy + các ký tự gốc)
    int totalBs = backspaceCount + (useDummy ? 1 : 0);
    for (int i = 0; i < totalBs && idx + 1 < Cay::MAX_BUFFER * 4 + 4; i++) {
        FillVkInput(&inputs[idx++], VK_BACK, 0);
        FillVkInput(&inputs[idx++], VK_BACK, KEYEVENTF_KEYUP);
    }

    // 3. INSERTION (Text mới)
    for (int i = 0; i < newTextLen && idx + 1 < Cay::MAX_BUFFER * 4 + 4; i++) {
        FillUnicodeInput(&inputs[idx++], newText[i], 0);
        FillUnicodeInput(&inputs[idx++], newText[i], KEYEVENTF_KEYUP);
    }

    // 4. PURE ATOMIC INJECTION (Gửi tất cả trong 1 tick)
    if (idx > 0) {
        SendInput((UINT)idx, inputs, sizeof(INPUT));
    }
}

} // namespace CayIME
