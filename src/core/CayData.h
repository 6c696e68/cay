#pragma once

#include "CayTypes.h"
#define MAX_BUFFER 64

namespace Cay {

// ---------------------------------------------------------------------------
// Tone mark indices (used as array subscripts throughout the engine).
//   0 = no tone (flat / ngang)
//   1 = huyền  (f)
//   2 = sắc    (s)
//   3 = hỏi    (r)
//   4 = ngã    (x)
//   5 = nặng   (j)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// CayData
//
// Pure static helper class: validation tables and tone-marked character maps.
// No instances, no dynamic allocation, no CRT.
// ---------------------------------------------------------------------------
class CayData {
public:
    // Returns true if the null-terminated string `s` of length `len` is a
    // recognised Vietnamese initial consonant cluster.
    static bool IsValidInitial(const wchar_t* s, int len);

    // Returns true if the null-terminated string `s` of length `len` is a
    // recognised Vietnamese vowel nucleus.
    static bool IsValidNucleus(const wchar_t* s, int len);

    // Map a Telex modifier key to a tone index (0–5).
    // Returns -1 if the key is not a tone key.
    static int  GetToneIndex(wchar_t key);

    // Return the tone-marked codepoint for a given base vowel + tone index.
    // Returns 0 if no mapping exists.
    static wchar_t GetToneMark(wchar_t base, int toneIndex);

    // Return true if `ch` is one of the Vietnamese-accented characters
    // (has a hook, circumflex, breve, or tone mark already applied).
    static bool HasVietnameseMark(wchar_t ch);

    // Return true if any character in `buf[0..len)` carries a Vietnamese mark.
    static bool HasVietnameseMark(const wchar_t* buf, int len);

    // Strip tone from a vowel, returning the plain ASCII base (a/e/i/o/u/y).
    // Returns `ch` unchanged if it is not a toned Vietnamese vowel.
    static wchar_t StripTone(wchar_t ch);

    // Strip hook/circumflex/breve from a vowel, returning the plain ASCII base.
    // Returns `ch` unchanged if it carries no such diacritic.
    static wchar_t StripAccent(wchar_t ch);

    // Return true if `ch` is any form of a Vietnamese vowel (plain or accented).
    static bool IsVowel(wchar_t ch);

    // Get hook rule for a vowel
    static wchar_t GetHookRule(wchar_t c);
};

} // namespace Cay
