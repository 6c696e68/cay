#pragma once
#include <cstdint>

namespace Cay {

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

    struct KeyEvent {
        KeyCode keyCode;
        wchar_t character; // Translated Unicode character
        bool handled;
    };

    typedef void (*InjectTextFunc)(int backspaceCount, const wchar_t* newText, int newTextLen);

    inline int CayStrLen(const wchar_t* s) {
        int len = 0;
        while (s && *s) { len++; s++; }
        return len;
    }
    inline int CayStrCmp(const wchar_t* s1, const wchar_t* s2) {
        while (*s1 && (*s1 == *s2)) { s1++; s2++; }
        return *s1 - *s2;
    }
} // namespace Cay

