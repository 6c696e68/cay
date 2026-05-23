#pragma once
#include <windows.h>

namespace CayIME {

class InputInjector {
public:
    // Sentinel value stamped on every synthetic event so our own hook ignores them.
    static const ULONG_PTR MAGIC_EXTRA_INFO = 0x1234;

    // Replace text at the current caret position.
    // Sends: [ZWJ dummy] + [backspaceCount x VK_BACK] + [newText characters]
    // all in a single SendInput call to prevent Chrome autocomplete races.
    static void ReplaceText(int backspaceCount, const wchar_t* newText, int newTextLen);
};

} // namespace CayIME
