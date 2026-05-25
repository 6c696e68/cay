#include "CayimeEngine.h"
#include <fcitx-utils/keysymgen.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/inputcontext.h>

// Global context pointer for the callback (since CayEngine uses a raw function pointer)
static fcitx::InputContext* g_current_ic = nullptr;

// Helper to convert wstring to utf8 string without deprecated wstring_convert
static std::string utf8_from_wstring(const std::wstring& wstr) {
    std::string result;
    for (wchar_t wc : wstr) {
        if (wc <= 0x7F) {
            result.push_back(static_cast<char>(wc));
        } else if (wc <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | ((wc >> 6) & 0x1F)));
            result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
        } else if (wc <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | ((wc >> 12) & 0x0F)));
            result.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | ((wc >> 18) & 0x07)));
            result.push_back(static_cast<char>(0x80 | ((wc >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
        }
    }
    return result;
}

static void GlobalInjectText(int backspaceCount, const wchar_t* newText, int newTextLen) {
    if (!g_current_ic) return;
    
    if (backspaceCount > 0) {
        bool supportsSurrounding = g_current_ic->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText);
        std::string prog = g_current_ic->program();
        
        // Many Linux terminals claim SurroundingText support but ignore deleteSurroundingText (VTE bugs).
        // We force fallback to raw Backspace keys for known terminal emulators.
        if (prog.find("terminal") != std::string::npos || 
            prog.find("alacritty") != std::string::npos ||
            prog.find("kitty") != std::string::npos ||
            prog.find("konsole") != std::string::npos ||
            prog.find("terminator") != std::string::npos ||
            prog.find("wezterm") != std::string::npos ||
            prog.find("tmux") != std::string::npos) {
            supportsSurrounding = false;
        }

        if (supportsSurrounding) {
            g_current_ic->deleteSurroundingText(-backspaceCount, backspaceCount);
        } else {
            for (int i = 0; i < backspaceCount; ++i) {
                g_current_ic->forwardKey(fcitx::Key(FcitxKey_BackSpace));
            }
        }
    }
    
    if (newTextLen > 0) {
        std::wstring wstr(newText, newTextLen);
        g_current_ic->commitString(utf8_from_wstring(wstr));
    }
}

CayimeEngine::CayimeEngine(fcitx::Instance* instance)
    : instance_(instance) {
    engine_.OnInjectText = GlobalInjectText;
}

CayimeEngine::~CayimeEngine() {
}

void CayimeEngine::reset(const fcitx::InputMethodEntry& /*entry*/, fcitx::InputContextEvent& /*event*/) {
    engine_.ResetFull();
}

bool CayimeEngine::convertKeyEvent(fcitx::KeyEvent& fcitxEvent, Cay::KeyEvent& cayEvent) {
    fcitx::Key key = fcitxEvent.key();
    
    if (key.sym() == FcitxKey_space) {
        cayEvent.keyCode = Cay::KeyCode::Space;
        cayEvent.character = ' ';
        cayEvent.handled = false;
        return true;
    }
    
    if (key.sym() > FcitxKey_space && key.sym() <= FcitxKey_asciitilde) {
        cayEvent.keyCode = static_cast<Cay::KeyCode>(toupper(key.sym()));
        cayEvent.character = static_cast<wchar_t>(key.sym());
        cayEvent.handled = false;
        return true;
    }
    
    if (key.sym() == FcitxKey_BackSpace) {
        cayEvent.keyCode = Cay::KeyCode::Backspace;
        cayEvent.character = 0;
        cayEvent.handled = false;
        return true;
    }
    if (key.sym() == FcitxKey_Return || key.sym() == FcitxKey_KP_Enter) { cayEvent.keyCode = Cay::KeyCode::Enter; return true; }
    if (key.sym() == FcitxKey_Escape) { cayEvent.keyCode = Cay::KeyCode::Escape; return true; }
    if (key.sym() == FcitxKey_Tab) { cayEvent.keyCode = Cay::KeyCode::Tab; return true; }
    if (key.sym() == FcitxKey_Left || key.sym() == FcitxKey_KP_Left) { cayEvent.keyCode = Cay::KeyCode::Left; return true; }
    if (key.sym() == FcitxKey_Right || key.sym() == FcitxKey_KP_Right) { cayEvent.keyCode = Cay::KeyCode::Right; return true; }
    if (key.sym() == FcitxKey_Up || key.sym() == FcitxKey_KP_Up) { cayEvent.keyCode = Cay::KeyCode::Up; return true; }
    if (key.sym() == FcitxKey_Down || key.sym() == FcitxKey_KP_Down) { cayEvent.keyCode = Cay::KeyCode::Down; return true; }
    if (key.sym() == FcitxKey_Home || key.sym() == FcitxKey_KP_Home) { cayEvent.keyCode = Cay::KeyCode::Home; return true; }
    if (key.sym() == FcitxKey_End || key.sym() == FcitxKey_KP_End) { cayEvent.keyCode = Cay::KeyCode::End; return true; }
    if (key.sym() == FcitxKey_Page_Up || key.sym() == FcitxKey_KP_Page_Up) { cayEvent.keyCode = Cay::KeyCode::PageUp; return true; }
    if (key.sym() == FcitxKey_Page_Down || key.sym() == FcitxKey_KP_Page_Down) { cayEvent.keyCode = Cay::KeyCode::PageDown; return true; }
    if (key.sym() == FcitxKey_Delete || key.sym() == FcitxKey_KP_Delete) { cayEvent.keyCode = Cay::KeyCode::Delete; return true; }
    
    return false;
}

void CayimeEngine::keyEvent(const fcitx::InputMethodEntry& /*entry*/, fcitx::KeyEvent& keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }
    
    fcitx::Key key = keyEvent.key();
    
    // Check modifiers using uint32_t cast to avoid enum class bitwise operator issues
    uint32_t states = static_cast<uint32_t>(key.states());
    uint32_t mask = static_cast<uint32_t>(fcitx::KeyState::Ctrl) | 
                    static_cast<uint32_t>(fcitx::KeyState::Alt) | 
                    static_cast<uint32_t>(fcitx::KeyState::Super);
                    
    if (states & mask) {
        engine_.ResetFull(); // Reset engine state on shortcuts (like Ctrl+A)
        return;
    }

    Cay::KeyEvent cayEvent;
    if (convertKeyEvent(keyEvent, cayEvent)) {
        g_current_ic = keyEvent.inputContext();
        
        engine_.OnKeyDown(cayEvent);
        
        if (cayEvent.handled) {
            keyEvent.filterAndAccept();
        }
        
        g_current_ic = nullptr;
    }
}
