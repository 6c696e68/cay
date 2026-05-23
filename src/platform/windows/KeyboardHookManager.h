#pragma once
#include <windows.h>
#include "InputInjector.h"

namespace CayIME {

// ---------------------------------------------------------------------------
// HookKeyEventArgs – passed to key-event callbacks.
// ---------------------------------------------------------------------------
struct HookKeyEventArgs {
    DWORD      keyCode;     // Virtual-key code (e.g. VK_A, VK_BACK …)
    wchar_t    character;   // Translated Unicode character (0 if none)
    ULONG_PTR  extraInfo;   // dwExtraInfo from the low-level hook
    bool       handled;     // Set true by handler to suppress the keystroke
};

// ---------------------------------------------------------------------------
// InputHookManager
//
// Installs WH_KEYBOARD_LL and WH_MOUSE_LL hooks.
// Callbacks are raw function pointers – no std::function, no heap allocation.
// Key debouncing uses a 256-bit bitmask (4 × DWORD64) keyed by VK code.
// ---------------------------------------------------------------------------
class InputHookManager {
public:
    // Callback signatures.
    using KeyCallback   = void (*)(InputHookManager* sender, HookKeyEventArgs& e);
    using MouseCallback = void (*)(InputHookManager* sender);

    KeyCallback   KeyDown   = nullptr;
    KeyCallback   KeyUp     = nullptr;
    MouseCallback MouseClick = nullptr;

    InputHookManager();
    ~InputHookManager();

private:
    HHOOK _kbHook;
    HHOOK _mouseHook;

    // 256-bit pressed-state bitmask.  Bit N = VK code N is currently down.
    DWORD64 _keyState[4];   // 4 × 64 = 256 bits

    void SetKeyBit(DWORD vk);
    void ClearKeyBit(DWORD vk);
    bool TestKeyBit(DWORD vk) const;

    static LRESULT CALLBACK KbProc  (int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    // Singleton pointer so static callbacks can reach the instance.
    static InputHookManager* s_instance;
};

} // namespace CayIME
