#include "InputInjector.h"

// Maximum characters in a single word + ZWJ + backspaces headroom.
// 1 ZWJ + 64 backspaces + 256 unicode chars = 321 INPUT structs worst case.
#define MAX_INPUTS 321

namespace CayIME {

// ---------------------------------------------------------------------------
// Internal helper – fill an INPUT for a Unicode keydown/keyup pair.
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
// Internal helper – fill an INPUT for a virtual-key keydown/keyup pair.
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
// ReplaceText – the core batched injection.
//
// Layout of the single INPUT array:
//   [0]      ZWJ down
//   [1]      ZWJ up
//   [2..2+2*bs-1]   bs*(VK_BACK down + VK_BACK up)
//   remaining       newText unicode pairs
// ---------------------------------------------------------------------------
void InputInjector::ReplaceText(int backspaceCount, const wchar_t* newText, int newTextLen) {
    if (backspaceCount <= 0 && newTextLen <= 0) return;

    INPUT inputs[256];
    int idx = 0;

    // 1. ZWJ DUMMY INJECTION (Chrome/Excel Autocomplete Breaker)
    // We only inject the dummy if we are actually replacing text (backspaceCount > 0).
    // 1. Dummy character to wake up the target window caret.
    // ZWJ (\u200D) and ZWSP (\u200B) are dropped by strict editors like GitHub/CodeMirror.
    // To be 100% bulletproof across all editors, we use a standard printable letter ('a').
    // Since it's inserted and immediately backspaced within the same OS event batch,
    // it never flashes on screen and safely clears any autocomplete selection.
    bool useDummy = (backspaceCount > 0);
    if (useDummy) {
        FillUnicodeInput(&inputs[idx++], L'a', 0);               // Dummy down
        FillUnicodeInput(&inputs[idx++], L'a', KEYEVENTF_KEYUP); // Dummy up
    }

    // 2. DELETION (Erase the dummy + the original characters)
    int totalBs = backspaceCount + (useDummy ? 1 : 0);
    for (int i = 0; i < totalBs && idx + 1 < 256; i++) {
        FillVkInput(&inputs[idx++], VK_BACK, 0);
        FillVkInput(&inputs[idx++], VK_BACK, KEYEVENTF_KEYUP);
    }

    // 3. INSERTION (New Text)
    for (int i = 0; i < newTextLen && idx + 1 < 256; i++) {
        FillUnicodeInput(&inputs[idx++], newText[i], 0);
        FillUnicodeInput(&inputs[idx++], newText[i], KEYEVENTF_KEYUP);
    }

    // 4. PURE ATOMIC INJECTION (Send everything in 1 tick)
    if (idx > 0) {
        SendInput((UINT)idx, inputs, sizeof(INPUT));
    }
}

} // namespace CayIME
