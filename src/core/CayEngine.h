#pragma once
#include "CayData.h"
#include "CayTypes.h"

namespace Cay {

// ---------------------------------------------------------------------------
// MyKey – compact record of one raw keystroke plus the character it produced
// in the output buffer (may differ from the raw key after transformation).
// ---------------------------------------------------------------------------
struct MyKey {
    wchar_t raw;       // The raw character typed by the user (e.g. 'a', 'w', 's')
    wchar_t output;    // The character currently sitting in the output text
};

// ---------------------------------------------------------------------------
// TelexEngine
//
// Main Telex processing state machine.
// Owns:
//   _buffer[MAX_BUFFER]  – raw key records
//   _bufferCount         – number of valid entries in _buffer
//   _text[MAX_BUFFER]    – the current output characters (what was injected)
//   _textLen             – length of _text
//
// All processing is done via backward-scan (RULE 3).
// No dynamic allocation, no STL, no CRT.
// ---------------------------------------------------------------------------
class TelexEngine {
public:
    TelexEngine();

    // Called on every keydown (main.cpp delegates here).
    void OnKeyDown(Cay::KeyEvent& e);

    // Called on every keyup (currently a no-op, reserved for future use).
    void OnKeyUp(Cay::KeyEvent& e);

    // Hard reset: flush buffer and discard all state.
    void ResetFull();

    // Commit current word: save state for recall, then reset.
    void CommitWord();

    InjectTextFunc OnInjectText = nullptr;

private:
    MyKey _buffer[MAX_BUFFER];
    int   _bufferCount;

    wchar_t _text[MAX_BUFFER];
    int     _textLen;

    // Current tone index (0–5) applied to this word, or -1 if none.
    int  _toneIndex;

    // Output tracking for precise screen updates (exact diff calculation)
    wchar_t _lastOutput[MAX_BUFFER];
    int     _lastOutputLen;

    // Word recall state
    MyKey   _savedBuffer[MAX_BUFFER];
    int     _savedBufferCount;
    wchar_t _savedText[MAX_BUFFER];
    int     _savedTextLen;
    int     _savedToneIndex;
    bool    _canRestore;

    void SaveState();

    // -----------------------------------------------------------------------
    // Core transformation steps – all mutate _text[] in place.
    // -----------------------------------------------------------------------

    // Process a double-key circumflex (aa->â, ee->ê, oo->ô, dd->đ).
    // Returns true if the key was consumed as a double-key modifier.
    bool ApplyDoubleKeys(wchar_t key);

    // Process a hook/breve key ('w').
    // Returns true if the key was consumed as a hook modifier.
    bool ApplyHookKeys(wchar_t key);

    // Apply (or change) a tone mark to the output buffer.
    // Returns true if a tone was applied.
    bool ApplyToneMarks(int toneIndex);

    // Strip all tone marks from _text[], rewriting in place.
    void StripAllTones();

    // Find the rightmost vowel index in _text[0.._textLen) that is a good
    // candidate for receiving a tone mark (Vietnamese tone placement rule).
    int FindTonePosition() const;

    // Update screen efficiently by calculating exact backspaces
    void UpdateScreen(const wchar_t* newOutput, int newOutputLen);

    // Commit the current word: inject _text[] to replace what the user sees.
    void Commit(int extraBs = 0);

    // Revert to ASCII raw input (English fallback).
    void FallbackToRaw();

    // Check whether the current buffer looks like an English word and should
    // bypass Vietnamese processing.
    bool ShouldBypassWord() const;

    // Reset internal state without sending any input.
    void ResetState();

    // Helpers
    static bool IsAlpha(wchar_t ch);
    static wchar_t ToLowerViet(wchar_t c);
    static wchar_t ToUpperViet(wchar_t c);
};

} // namespace Cay
