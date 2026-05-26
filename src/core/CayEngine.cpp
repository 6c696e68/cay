#include "CayEngine.h"

// ---------------------------------------------------------------------------
// Compile-time guard: khoá `MAX_BUFFER` ở 64 để ngăn hồi quy đa nền tảng
// (Requirement 5.1, 5.2, 5.3). Nếu ai đó vô tình đổi giá trị trong CayTypes.h
// hoặc redefine bằng `#define`, build sẽ fail ngay tại compile-time thay vì
// gây silent truncation ở MacInputInjector / Windows InputInjector / Fcitx5.
// Tham chiếu: tasks.md task 6.5 — "Verify Phase 5 — build all 3 platform pass".
// ---------------------------------------------------------------------------
static_assert(Cay::MAX_BUFFER == 64,
              "Cay::MAX_BUFFER must remain 64 — see tasks.md task 6.5");

// ============================================================================
// CayEngine.cpp  –  Free-style Telex state machine (RULE 3)
//
// Tổng quan kiến trúc
// ---------------------
// Engine duy trì hai array song song:
//   _buffer[_bufferCount]  – mọi phím thô user đã gõ
//   _text[_textLen]        – Unicode output đã được inject đến hiện tại
//
// Trên mỗi keydown engine:
//   1. Kiểm tra các phím đặc biệt/control (Backspace, Escape, Space, etc.)
//   2. Thử áp dụng phím như modifier dấu mũ đôi.
//   3. Thử áp dụng phím như modifier dấu mũ/dấu ngắn ('w').
//   4. Thử áp dụng phím như dấu thanh (s/f/r/x/j/z).
//   5. Fallback về thêm ký tự như chữ cái thuần.
//
// Sau mỗi mutation, _text đã được update được inject qua InputInjector::ReplaceText.
// ============================================================================

namespace Cay {

// ---------------------------------------------------------------------------
// Static helpers - Helper static
// ---------------------------------------------------------------------------
bool TelexEngine::IsAlpha(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
}

// ---------------------------------------------------------------------------
// Constructor - Hàm khởi tạo
//
// Zero-init TẤT CẢ field nội bộ — bao gồm cả region saved-state
// (`_savedBuffer/_savedBufferCount/_savedText/_savedTextLen/_savedToneIndex`)
// và phần đuôi của `_lastOutput[1..MAX_BUFFER-1]` mà `ResetState()` không
// chạm tới (`ResetState` chỉ set `_lastOutput[0]=0` làm null-terminator).
// Property 12 (test/test_idempotency.cpp) yêu cầu engine vừa khởi tạo phải
// có toàn bộ field nội bộ ở giá trị xác định để `ResetFull()` có thể tương
// đương fresh construction (Requirement 10.3). Stack garbage ở bất kỳ
// field nào sẽ làm hai engine fresh khác nhau.
// ---------------------------------------------------------------------------
TelexEngine::TelexEngine() {
    _canRestore = false;
    _savedBufferCount = 0;
    _savedTextLen     = 0;
    _savedToneIndex   = -1;
    for (int i = 0; i < MAX_BUFFER; i++) {
        _savedBuffer[i].raw    = 0;
        _savedBuffer[i].output = 0;
        _savedText[i]          = 0;
        _lastOutput[i]         = 0;
    }
    ResetState();
}

// ---------------------------------------------------------------------------
// ResetState – zero tất cả mà không chạm vào display.
// ---------------------------------------------------------------------------
void TelexEngine::ResetState() {
    _bufferCount = 0;
    _textLen     = 0;
    _toneIndex   = -1;
    _lastOutputLen = 0;
    _lastOutput[0] = L'\0';
    for (int i = 0; i < MAX_BUFFER; i++) {
        _buffer[i].raw    = 0;
        _buffer[i].output = 0;
        _text[i]          = 0;
    }
}

// ---------------------------------------------------------------------------
// ResetFull – discard buffer và invalidate recall state.
//
// Khác với `ResetState()` (chỉ clear active state, được dùng bởi
// `CommitWord()` để PRESERVE saved-state cho recall), `ResetFull()` clear
// CẢ saved-state region và phần đuôi `_lastOutput[1..MAX_BUFFER-1]` mà
// `ResetState()` không chạm tới — để engine trở về trạng thái tương đương
// fresh-construction (Requirement 10.3, Property 12).
// ---------------------------------------------------------------------------
void TelexEngine::ResetFull() {
    ResetState();
    _canRestore       = false;
    _savedBufferCount = 0;
    _savedTextLen     = 0;
    _savedToneIndex   = -1;
    for (int i = 0; i < MAX_BUFFER; i++) {
        _savedBuffer[i].raw    = 0;
        _savedBuffer[i].output = 0;
        _savedText[i]          = 0;
        _lastOutput[i]         = 0;
    }
}

// ---------------------------------------------------------------------------
// SaveState – cache state từ hiện tại để recall.
// ---------------------------------------------------------------------------
void TelexEngine::SaveState() {
    if (_bufferCount > 0) {
        _savedBufferCount = _bufferCount;
        for (int i = 0; i < _bufferCount; i++) _savedBuffer[i] = _buffer[i];
        
        _savedTextLen = _textLen;
        for (int i = 0; i < _textLen; i++) _savedText[i] = _text[i];
        
        _savedToneIndex = _toneIndex;
        _canRestore = true;
    }
}

// ---------------------------------------------------------------------------
// CommitWord – lưu state để recall, sau đó reset.
// ---------------------------------------------------------------------------
void TelexEngine::CommitWord() {
    SaveState();
    ResetState();
}

// ---------------------------------------------------------------------------
// ReplayKey – Xử lý 1 phím thô: record vào _buffer, thử modifiers, fallback.
// Dùng chung cho OnKeyDown chính và Backspace replay.
// ---------------------------------------------------------------------------
void TelexEngine::ReplayKey(wchar_t ch) {
    wchar_t lo = CayData::ToLowerViet(ch);

    _buffer[_bufferCount].raw    = ch;
    _buffer[_bufferCount].output = ch;
    _bufferCount++;

    bool bypass = ShouldBypassWord();
    bool appliedModifier = false;
    bool mutatedVowel    = false; // true nếu modifier biến đổi nguyên âm

    if (!bypass && (lo == L'a' || lo == L'e' || lo == L'o' || lo == L'd')) {
        if (_textLen > 0 && ApplyDoubleKeys(ch)) {
            appliedModifier = true;
            mutatedVowel    = true;
        }
    }
    if (!bypass && !appliedModifier && lo == L'w' && _textLen > 0) {
        if (ApplyHookKeys(ch)) {
            appliedModifier = true;
            mutatedVowel    = true;
        }
    }
    if (!bypass && !appliedModifier) {
        int ti = CayData::GetToneIndex(lo);
        if (ti >= 0 && _textLen > 0) {
            if (ApplyToneMarks(ti)) appliedModifier = true;
        }
    }
    if (!appliedModifier) {
        if (_textLen < MAX_BUFFER - 1) {
            _text[_textLen++] = ch;
            _text[_textLen]   = L'\0';
        }
    }

    // Sau khi mutate cấu trúc âm tiết (double/hook key biến đổi nguyên âm,
    // hoặc append một phụ âm cuối), re-position dấu thanh nếu vị trí cũ
    // không còn đúng theo quy tắc tiếng Việt. Idempotent — no-op nếu không
    // có dấu hoặc đã đúng vị trí.
    bool appendedConsonant = !appliedModifier
                          && IsAlpha(ch)
                          && !CayData::IsVowel(ch);
    if (mutatedVowel || appendedConsonant) {
        RepositionTone();
    }
}

void TelexEngine::UpdateScreen(const wchar_t* newOutput, int newOutputLen) {
    // 1. Kiểm tra xem có giống nhau không
    bool isIdentical = (_lastOutputLen == newOutputLen);
    if (isIdentical) {
        for (int i = 0; i < newOutputLen; i++) {
            if (_lastOutput[i] != newOutput[i]) {
                isIdentical = false;
                break;
            }
        }
    }
    if (isIdentical) return;

    // 2. Tìm prefix chung
    int commonPrefixLen = 0;
    int minLen = _lastOutputLen < newOutputLen ? _lastOutputLen : newOutputLen;
    for (int i = 0; i < minLen; i++) {
        if (_lastOutput[i] == newOutput[i]) {
            commonPrefixLen++;
        } else {
            break;
        }
    }

    // 3. Tính toán diff chính xác mà không có padding giả
    int backspacesNeeded = _lastOutputLen - commonPrefixLen;
    const wchar_t* textToType = newOutput + commonPrefixLen;
    int textToTypeLen = newOutputLen - commonPrefixLen;

    // 4. Inject phím chính xác
    if (backspacesNeeded > 0 || textToTypeLen > 0) {
        if (OnInjectText) OnInjectText(backspacesNeeded, textToType, textToTypeLen);
    }

    // 5. Update state
    for (int i = 0; i < newOutputLen; i++) {
        _lastOutput[i] = newOutput[i];
    }
    _lastOutput[newOutputLen] = L'\0';
    _lastOutputLen = newOutputLen;
}

// ---------------------------------------------------------------------------
// FallbackToRaw – revert về các ký tự ASCII thô user đã gõ.
// Được gọi khi engine quyết định input là tiếng Anh.
// ---------------------------------------------------------------------------
void TelexEngine::FallbackToRaw() {
    // Xây chuỗi ASCII thô từ _buffer.
    wchar_t raw[MAX_BUFFER];
    int rawLen = 0;
    for (int i = 0; i < _bufferCount && rawLen < MAX_BUFFER - 1; i++) {
        raw[rawLen++] = _buffer[i].raw;
    }
    raw[rawLen] = L'\0';

    if (OnInjectText) OnInjectText(_textLen, raw, rawLen);

    // Update _text để phản ánh fallback.
    for (int i = 0; i < rawLen; i++) _text[i] = raw[i];
    _textLen = rawLen;
    _toneIndex = -1;
    _canRestore = false;
}

// ---------------------------------------------------------------------------
// LEVEL 2: Validator cấu trúc — Kiểm tra âm tiết bằng `CayData::TryMatch*`.
//
// Các bảng âm tiết (initials/nuclei/finals/tails) đã được single-source-of-truth
// hoá tại `CayData.cpp` (Requirement 2). Hàm này chỉ giữ logic đặc tả tiếng Việt:
//   - Special case "gi" rollback: nếu sau "gi" không phải nguyên âm thì 'i' là
//     nucleus (vd. "gì", "gìn").
//   - Nucleus-Final Pairing Rule: nh/ch chỉ đi với a/i/ê/y/oa/uy/uê;
//     ng/c không đi với i/ê/y.
// ---------------------------------------------------------------------------
#ifdef CAY_TEST_BUILD
bool IsCompleteSyllable(const wchar_t* s, int len) {
#else
static bool IsCompleteSyllable(const wchar_t* s, int len) {
#endif
    if (len <= 0 || len > 20) return false;

    int pos = 0;

    // ── BLOCK 1: Phụ âm đầu (có thể rỗng) ───────────────────────────────
    int initialLen = CayData::TryMatchInitial(s + pos, len - pos);
    pos += initialLen;

    // Special case "gi" rollback: nếu sau "gi" không phải nguyên âm thì
    // 'i' là nucleus (vd. "gì" → initial="g", nucleus="i").
    if (initialLen == 2 && s[0] == L'g' && s[1] == L'i') {
        if (pos == len || !CayData::IsVowel(CayData::StripTone(s[pos]))) {
            pos--; // rollback: 'i' chuyển thành nucleus
        }
    }

    // ── BLOCK 2: Nhân nguyên âm (bắt buộc) ─────────────────────────────
    int nucleusLen = CayData::TryMatchNucleus(s + pos, len - pos);
    if (nucleusLen == 0) return false;
    const wchar_t* nucleusPtr = s + pos;
    pos += nucleusLen;

    // ── BLOCK 3: Phụ âm cuối (tuỳ chọn) + Nucleus-Final Pairing Rule ──
    int finalLen = CayData::TryMatchFinal(s + pos, len - pos);
    if (finalLen > 0) {
        // So nucleus slice (length `nucleusLen` tại `nucleusPtr`) với literal.
        auto nucEq = [&](const wchar_t* lit) -> bool {
            int litLen = 0;
            while (lit[litLen]) litLen++;
            if (litLen != nucleusLen) return false;
            for (int i = 0; i < litLen; i++) {
                if (nucleusPtr[i] != lit[i]) return false;
            }
            return true;
        };

        // Quy tắc 1: nh, ch CHỈ đi với a, i, ê, y, oa, uy, uê.
        bool finalIsNhCh = (finalLen == 2 && s[pos] == L'c' && s[pos + 1] == L'h')
                        || (finalLen == 2 && s[pos] == L'n' && s[pos + 1] == L'h');
        if (finalIsNhCh) {
            bool valid = nucEq(L"a") || nucEq(L"i") || nucEq(L"\u00ea") || nucEq(L"y")
                      || nucEq(L"oa") || nucEq(L"uy") || nucEq(L"u\u00ea");
            if (!valid) return false;
        }

        // Quy tắc 2: ng, c KHÔNG đi với i, ê, y (lưu ý iê/yê/uô/ươ vẫn hợp lệ
        // vì chúng được match như nucleus 2-3 ký tự, không phải "i"/"ê"/"y").
        bool finalIsNgC = (finalLen == 2 && s[pos] == L'n' && s[pos + 1] == L'g')
                       || (finalLen == 1 && s[pos] == L'c');
        if (finalIsNgC) {
            bool invalid = nucEq(L"i") || nucEq(L"\u00ea") || nucEq(L"y");
            if (invalid) return false;
        }

        pos += finalLen;
    }

    // ── BLOCK 4: Vần phụ (tuỳ chọn) ───────────────────────────────────
    pos += CayData::TryMatchTail(s + pos, len - pos);

    return pos == len;
}

// ---------------------------------------------------------------------------
// ShouldBypassWord
//
// Trả về true nếu sequence hiện tại trông giống tiếng Anh và nên bypass
// transform tiếng Việt.
// ---------------------------------------------------------------------------
bool TelexEngine::ShouldBypassWord() const {
    if (_bufferCount == 0) return false;

    // Trích xuất các phím gốc đã gõ (chuyển về chữ thường để dễ check)
    wchar_t raw[16] = {0};
    int len = _bufferCount < 15 ? _bufferCount : 15;
    for(int i = 0; i < len; i++) {
        raw[i] = CayData::ToLowerViet(_buffer[i].raw);
    }

    // ── CẢI TIẾN 1: Strict Tone-Final Consonant Rule ──
    // Đưa lên đầu để bắt ngay các ngoại lệ dù đã có dấu tiếng Việt trước đó
    if (_textLen >= 1) {
        wchar_t textLo[MAX_BUFFER];
        for (int i = 0; i < _textLen; i++) {
            textLo[i] = CayData::ToLowerViet(CayData::StripTone(_text[i]));
        }
        textLo[_textLen] = L'\0';

        wchar_t lastC = textLo[_textLen - 1];
        bool endsWithPtkc = false;
        
        // Kiểm tra kết thúc bằng c, p, t hoặc ch
        if (lastC == L'c' || lastC == L'p' || lastC == L't') {
            endsWithPtkc = true;
        } else if (_textLen >= 2 && textLo[_textLen - 2] == L'c' && textLo[_textLen - 1] == L'h') {
            endsWithPtkc = true;
        }
        
        if (endsWithPtkc) {
            // Âm tiết kết thúc bằng c, ch, p, t đang mang dấu Huyền(1), Hỏi(3), Ngã(4)
            if (_toneIndex == 1 || _toneIndex == 3 || _toneIndex == 4) return true;
            
            // Hoặc phím vừa gõ chuẩn bị tạo ra dấu Huyền(f), Hỏi(r), Ngã(x)
            if (len > 0) {
                wchar_t lastKey = raw[len - 1];
                if (lastKey == L'f' || lastKey == L'r' || lastKey == L'x') return true;
            }
        }
    }

    // Bỏ qua nếu đã có dấu tiếng Việt hợp lệ
    if (CayData::HasVietnameseMark(_text, _textLen)) return false;

    // LEVEL 1: Hard Filter — Cụm phụ âm đầu không hợp lệ
    if (raw[0] == L'w' || raw[0] == L'f' || raw[0] == L'j' || raw[0] == L'z') return true;

    if (len >= 2) {
        // Luật Q: Bắt buộc đi với u
        if (raw[0] == L'q' && raw[1] != L'u') return true;

        // Luật P: Bắt buộc đi với h (bỏ qua các từ mượn như pin, paté, public, padding,...)
        if (raw[0] == L'p' && raw[1] != L'h') return true;

        // Luật Phụ âm kép: Tiếng Việt chỉ có 8 cặp phụ âm kép hợp lệ ở đầu từ.
        // Helper: Kiểm tra xem ký tự có phải là phụ âm ASCII không
        auto isConsonant = [](wchar_t c) {
            return (c >= L'a' && c <= L'z') && 
                   (c != L'a' && c != L'e' && c != L'i' && c != L'o' && c != L'u' && c != L'y');
        };

        if (isConsonant(raw[0]) && isConsonant(raw[1])) {
            bool validVietCluster = 
                (raw[0] == L'c' && raw[1] == L'h') ||
                (raw[0] == L'g' && raw[1] == L'h') ||
                (raw[0] == L'k' && raw[1] == L'h') ||
                (raw[0] == L'n' && (raw[1] == L'g' || raw[1] == L'h')) ||
                (raw[0] == L'p' && raw[1] == L'h') ||
                (raw[0] == L't' && (raw[1] == L'h' || raw[1] == L'r')) ||
                (raw[0] == L'd' && raw[1] == L'd'); // <--- BỔ SUNG NGOẠI LỆ CHO CHỮ "đ" TẠI ĐÂY
            
            // Nếu là 2 phụ âm đứng đầu nhưng không nằm trong danh sách trên -> 100% English (vd: class, style, block)
            if (!validVietCluster) return true;
        }
        if (raw[0] == L'c' && (raw[1] == L'i' || raw[1] == L'e' || raw[1] == L'\u00EA' || raw[1] == L'y')) return true;
        if (raw[0] == L'k' && !(raw[1] == L'h' || raw[1] == L'i' || raw[1] == L'e' || raw[1] == L'\u00EA' || raw[1] == L'y')) return true;
        if (raw[0] == L'g' && (raw[1] == L'e' || raw[1] == L'\u00EA' || raw[1] == L'y')) return true;
        if (len >= 3 && raw[0] == L'g' && raw[1] == L'h') {
            if (raw[2] != L'i' && raw[2] != L'e' && raw[2] != L'\u00EA' && raw[2] != L'y') return true;
        }
        if (len >= 3 && raw[0] == L'n' && raw[1] == L'g' && raw[2] != L'h') {
            if (raw[2] == L'i' || raw[2] == L'e' || raw[2] == L'\u00EA' || raw[2] == L'y') return true;
        }
        if (len >= 4 && raw[0] == L'n' && raw[1] == L'g' && raw[2] == L'h') {
            if (raw[3] != L'i' && raw[3] != L'e' && raw[3] != L'\u00EA' && raw[3] != L'y') return true;
        }
    }

    // LEVEL 2: Structural Validator
    bool hasVowel = false;
    for (int i = 0; i < _textLen; i++) {
        if (CayData::IsVowel(_text[i])) { hasVowel = true; break; }
    }
    
    if (hasVowel) {
        wchar_t textLo[MAX_BUFFER];
        for (int i = 0; i < _textLen; i++) {
            textLo[i] = CayData::ToLowerViet(CayData::StripTone(_text[i]));
        }
        textLo[_textLen] = L'\0';
        
        if (!IsCompleteSyllable(textLo, _textLen)) return true;
    } else {
        if (_textLen >= 5) return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// FindTonePosition
//
// Returns the index in _text[] where the tone mark should be placed.
// ---------------------------------------------------------------------------
int TelexEngine::FindTonePosition() const {
    if (_textLen == 0) return -1;

    // Collect vowel positions.
    int first = -1, last = -1, count = 0;
    for (int i = 0; i < _textLen; i++) {
        if (CayData::IsVowel(_text[i])) {
            if (first == -1) first = i;
            last = i;
            count++;
        }
    }
    if (count == 0) return -1;
    if (count == 1) return first;

    // Does a consonant follow the last vowel?
    bool hasConsonantFinal = false;
    for (int i = last + 1; i < _textLen; i++) {
        if (IsAlpha(_text[i]) && !CayData::IsVowel(_text[i])) {
            hasConsonantFinal = true;
            break;
        }
    }

    // Helper: get plain ASCII base of a potentially toned/hooked vowel.
    auto baseVowel = [](wchar_t c) -> wchar_t {
        return CayData::ToLowerViet(CayData::StripAccent(CayData::StripTone(c)));
    };

    if (count == 2) {
        if (hasConsonantFinal) return last; // e.g. "tuấn", "điện"

        wchar_t v1 = baseVowel(_text[first]);
        wchar_t v2 = baseVowel(_text[last]);

        // Helper: nguyên âm có dấu mũ/móc (â, ê, ô, ơ, ă, ư) — đã strip tone
        // qua DecomposeChar.base, không qua baseVowel.
        auto isHooked = [](wchar_t c) -> bool {
            return c == L'\u00e2' || c == L'\u00ea' || c == L'\u00f4'
                || c == L'\u01a1' || c == L'\u0103' || c == L'\u01b0';
        };
        wchar_t b1 = CayData::DecomposeChar(_text[first]).base;
        wchar_t b2 = CayData::DecomposeChar(_text[last]).base;

        // Ưu tiên cao nhất: nguyên âm có dấu mũ/móc luôn nhận dấu thanh
        // (vd. "uâ" → â, "uô" → ô, "uơ" → ơ, "iâ" → â, "uă" → ă, ...).
        // Quy tắc tiếng Việt: dấu thanh đặt trên nguyên âm mũ/móc nếu có.
        if (isHooked(b2) && !isHooked(b1)) return last;
        if (isHooked(b1) && !isHooked(b2)) return first;

        // oa, oe
        if (v1 == L'o' && (v2 == L'a' || v2 == L'e')) return last;
        // uê, uy, uơ
        if (v1 == L'u' && (v2 == L'e' || v2 == L'y' || v2 == L'o')) return last;
        // iê
        if (v1 == L'i' && v2 == L'e') return last;

        // qu + vowel (e.g. quá)
        if (v1 == L'u' && first > 0 && CayData::ToLowerViet(_text[first - 1]) == L'q') return last;
        // gi + vowel (e.g. già)
        if (v1 == L'i' && first > 0 && CayData::ToLowerViet(_text[first - 1]) == L'g') return last;

        // Default for open 2-vowel syllable: first vowel (e.g. rồi, mèo, đôi, bơi, múa)
        return first;
    }

    if (count >= 3) {
        if (hasConsonantFinal) return last; // e.g. "tuyến", "giường"

        wchar_t v1 = baseVowel(_text[first]);
        wchar_t v2 = baseVowel(_text[first + 1]);
        wchar_t v3 = baseVowel(_text[last]);

        // Ưu tiên: nguyên âm có dấu mũ/móc trong cụm 3 nguyên âm mở.
        // Vd. "uyê" (incomplete "nguyễ") → ê; "uô" trong "giuô" → ô.
        auto isHooked = [](wchar_t c) -> bool {
            return c == L'\u00e2' || c == L'\u00ea' || c == L'\u00f4'
                || c == L'\u01a1' || c == L'\u0103' || c == L'\u01b0';
        };
        wchar_t b1 = CayData::DecomposeChar(_text[first]).base;
        wchar_t b2 = CayData::DecomposeChar(_text[first + 1]).base;
        wchar_t b3 = CayData::DecomposeChar(_text[last]).base;
        if (isHooked(b3) && !isHooked(b2) && !isHooked(b1)) return last;
        if (isHooked(b2) && !isHooked(b1) && !isHooked(b3)) return first + 1;
        if (isHooked(b1) && !isHooked(b2) && !isHooked(b3)) return first;

        // uyê (incomplete "nguyễ", "chuyế") -> tone on ê
        if (v1 == L'u' && v2 == L'y' && v3 == L'e') return last;
        // giuô, giươ (incomplete "giuộ", "giượ") -> tone on ô/ơ
        if (v1 == L'i' && v2 == L'u' && v3 == L'o') return last;

        // Default 3-vowel: middle vowel (e.g. người, ngoài, khuya)
        return first + 1;
    }

    return last;
}


// ---------------------------------------------------------------------------
// StripAllTones – rewrite _text[] removing any tone mark from every vowel.
// ---------------------------------------------------------------------------
void TelexEngine::StripAllTones() {
    for (int i = 0; i < _textLen; i++) {
        wchar_t stripped = CayData::StripTone(_text[i]);
        _text[i] = stripped;
    }
}

// ---------------------------------------------------------------------------
// RepositionTone – di chuyển dấu thanh sang đúng vị trí theo cấu trúc
// âm tiết hiện tại.
//
// Quét _text[]: nếu thấy nguyên âm có dấu thanh ở vị trí khác với
// `FindTonePosition()` thì chuyển dấu sang vị trí mới. Idempotent:
// nếu đã đúng vị trí thì no-op.
//
// Mục đích: xử lý fail-to-reposition bug khi:
//   1. User gõ "xua" + "s" → "xúa" (sắc trên u — đúng cho cụm mở "ua").
//      Sau đó gõ "t" → "xúat" → cấu trúc đổi (có phụ âm cuối) →
//      dấu phải dời sang "a" thành "xuát".
//   2. User gõ "xua" + "s" + "a"(double-key) → "xuấa"? Không, ApplyDoubleKeys
//      biến "a" thành "â": "xúâ". Lúc này cụm "uâ" mở → ưu tiên hooked vowel
//      → dấu dời từ "u" sang "â" thành "xuấ".
//   3. Gõ tiếp "t" → "xuất" (cấu trúc khép, dấu vẫn ở "â" — đúng).
// ---------------------------------------------------------------------------
void TelexEngine::RepositionTone() {
    if (_textLen == 0) return;

    // Tìm vị trí dấu hiện tại (nếu có).
    int currentPos = -1;
    int currentTone = 0;
    for (int i = 0; i < _textLen; i++) {
        auto d = CayData::DecomposeChar(_text[i]);
        if (d.toneIndex > 0) {
            currentPos = i;
            currentTone = d.toneIndex;
            break;
        }
    }
    if (currentPos < 0) return; // không có dấu → không cần làm gì

    int targetPos = FindTonePosition();
    if (targetPos < 0 || targetPos == currentPos) return; // đã đúng

    // Strip dấu cũ.
    auto dOld = CayData::DecomposeChar(_text[currentPos]);
    _text[currentPos] = CayData::ComposeChar(dOld.base, 0, dOld.isUpper);

    // Đặt dấu vào vị trí mới.
    auto dNew = CayData::DecomposeChar(_text[targetPos]);
    wchar_t toned = CayData::ComposeChar(dNew.base, currentTone, dNew.isUpper);
    if (toned != L'\0') {
        _text[targetPos] = toned;
    } else {
        // Fallback: nếu không có mapping, đặt lại dấu cũ để giữ thông tin.
        _text[currentPos] = CayData::ComposeChar(dOld.base, currentTone, dOld.isUpper);
    }
}

// ---------------------------------------------------------------------------
// Bảng quy tắc cho double-keys (aa/ee/oo/dd):
//   loKey       : phím trigger (đã lowercase)
//   applyBase   : nguyên âm target khi apply (â/ê/ô/đ)
//   plainBase   : nguyên âm thuần để compare (a/e/o/d) khi apply hoặc undo
//   altUndoBase : nguyên âm thứ 2 cũng được undo (ă cho 'a', ơ cho 'o'); 0 nếu không
// ---------------------------------------------------------------------------
struct DoubleKeyRule {
    wchar_t loKey;
    wchar_t applyBase;
    wchar_t plainBase;
    wchar_t altUndoBase;
};
static const DoubleKeyRule s_doubleKeyRules[4] = {
    { L'a', 0x00E2, L'a', 0x0103 }, // a → â (apply); undo â/ă → a
    { L'e', 0x00EA, L'e', 0      }, // e → ê (apply); undo ê → e
    { L'o', 0x00F4, L'o', 0x01A1 }, // o → ô (apply); undo ô/ơ → o
    { L'd', 0x0111, L'd', 0      }  // d → đ (apply); undo đ → d
};

bool TelexEngine::ApplyDoubleKeys(wchar_t key) {
    wchar_t loKey = CayData::ToLowerViet(key);

    // Tìm rule khớp loKey (chỉ 4 entries, linear OK).
    const DoubleKeyRule* rule = nullptr;
    for (int i = 0; i < 4; i++) {
        if (s_doubleKeyRules[i].loKey == loKey) { rule = &s_doubleKeyRules[i]; break; }
    }
    if (!rule) return false;

    // Helper: append `key` raw vào cuối _text, bảo vệ overflow.
    auto appendRaw = [&]() {
        if (_textLen < MAX_BUFFER - 1) { _text[_textLen++] = key; _text[_textLen] = L'\0'; }
    };

    // Backward-scan: tìm nguyên âm gần nhất ở cuối _text có thể nhận double-key.
    // Quy tắc Telex Tell-Don't-Ask: chỉ ký tự đầu tiên match (undo hoặc apply)
    // mới được transform; gặp phụ âm thường (không phải d/đ) thì dừng scan.
    for (int j = _textLen - 1; j >= 0; j--) {
        auto d = CayData::DecomposeChar(_text[j]);

        // 1. Undo logic — gõ lại double-key trên nguyên âm ĐÃ có dấu mũ/stroke
        //    → trả về nguyên âm thuần và append `key` raw vào cuối.
        if (d.base == rule->applyBase ||
            (rule->altUndoBase != 0 && d.base == rule->altUndoBase)) {
            _text[j] = CayData::ComposeChar(rule->plainBase, d.toneIndex, d.isUpper);
            appendRaw();
            return true;
        }

        // 2. Apply logic — gõ double-key trên nguyên âm thuần → thêm dấu mũ/stroke,
        //    giữ nguyên dấu thanh và case hiện tại.
        if (d.base == rule->plainBase) {
            _text[j] = CayData::ComposeChar(rule->applyBase, d.toneIndex, d.isUpper);
            return true;
        }

        // Dừng scan khi gặp phụ âm thường (không phải d/đ — d/đ vẫn là target hợp lệ).
        if (!CayData::IsVowel(d.base) && d.base != L'd' && d.base != L'\u0111') {
            break;
        }
    }
    return false;
}

bool TelexEngine::ApplyHookKeys(wchar_t key) {
    if (CayData::ToLowerViet(key) != L'w') return false;

    // Backward-scan: tìm nguyên âm gần nhất ở cuối _text có thể nhận hook.
    // Quy tắc Telex Tell-Don't-Ask: dùng DecomposeChar để bóc (base, tone, isUpper)
    // O(1) thay cho pattern quét tone qua vòng lặp t=1..5 GetToneMark.
    for (int j = _textLen - 1; j >= 0; j--) {
        auto d = CayData::DecomposeChar(_text[j]);

        // 1. Undo logic — gõ lại 'w' trên nguyên âm ĐÃ có hook (ă/ơ/ư)
        //    → bỏ hook và append `key` raw vào cuối.
        if (d.base == L'\u0103' || d.base == L'\u01a1' || d.base == L'\u01b0') {
            // Special case: nếu là 'ơ' và trước đó là 'ư' → undo cụm "ươ" thành "uo".
            if (d.base == L'\u01a1' && j > 0) {
                auto p = CayData::DecomposeChar(_text[j-1]);
                if (p.base == L'\u01b0') {
                    _text[j-1] = CayData::ComposeChar(L'u', p.toneIndex, p.isUpper);
                    _text[j]   = CayData::ComposeChar(L'o', d.toneIndex, d.isUpper);
                    if (_textLen < MAX_BUFFER - 1) { _text[_textLen++] = key; _text[_textLen] = L'\0'; }
                    return true;
                }
            }
            // Standard undo: ă→a, ơ→o, ư→u (giữ nguyên dấu thanh và case).
            wchar_t newBase = (d.base == L'\u0103') ? L'a'
                            : (d.base == L'\u01a1') ? L'o'
                                                    : L'u';
            _text[j] = CayData::ComposeChar(newBase, d.toneIndex, d.isUpper);
            if (_textLen < MAX_BUFFER - 1) { _text[_textLen++] = key; _text[_textLen] = L'\0'; }
            return true;
        }

        // Undo "ưu": gặp 'u' theo sau 'ư' → undo prev ư→u, append `key` raw.
        if (d.base == L'u' && j > 0) {
            auto p = CayData::DecomposeChar(_text[j-1]);
            if (p.base == L'\u01b0') {
                _text[j-1] = CayData::ComposeChar(L'u', p.toneIndex, p.isUpper);
                if (_textLen < MAX_BUFFER - 1) { _text[_textLen++] = key; _text[_textLen] = L'\0'; }
                return true;
            }
        }

        // 2. Apply logic
        // 2a. uo → ươ (trừ "quo" — giữ 'u' nguyên trong cụm qu).
        if (d.base == L'o' && j > 0) {
            auto p = CayData::DecomposeChar(_text[j-1]);
            if (p.base == L'u') {
                bool isQu = (j >= 2 && CayData::ToLowerViet(_text[j-2]) == L'q');
                if (!isQu) {
                    _text[j-1] = CayData::ComposeChar(L'\u01b0', p.toneIndex, p.isUpper); // ư
                    _text[j]   = CayData::ComposeChar(L'\u01a1', d.toneIndex, d.isUpper); // ơ
                    return true;
                }
            }
        }

        // 2b. ua → ưa (trừ "qua").
        if (d.base == L'a' && j > 0) {
            auto p = CayData::DecomposeChar(_text[j-1]);
            if (p.base == L'u') {
                bool isQu = (j >= 2 && CayData::ToLowerViet(_text[j-2]) == L'q');
                if (!isQu) {
                    _text[j-1] = CayData::ComposeChar(L'\u01b0', p.toneIndex, p.isUpper); // ư
                    return true;
                }
            }
        }

        // 2c. uu → ưu.
        if (d.base == L'u' && j > 0) {
            auto p = CayData::DecomposeChar(_text[j-1]);
            if (p.base == L'u') {
                _text[j-1] = CayData::ComposeChar(L'\u01b0', p.toneIndex, p.isUpper); // ư
                return true;
            }
        }

        // Cải tiến: Nếu chữ 'u' đứng cuối nhưng trước nó là 1 nguyên âm khác (vd: ou, au, eu, iu)
        // Thì không bao giờ nó nhận hook 'w' để thành 'ư' (trừ trường hợp 'uu' đã bắt ở trên).
        // Bỏ qua để vòng lặp lùi lại xử lý nguyên âm đứng trước (nhờ đó 'huou' + 'w' -> 'hươu').
        // Ngoại lệ: cụm `gi` là phụ âm đầu (giống `qu`) — `i` đóng vai trò consonant cluster,
        // 'u' sau `gi` VẪN nhận hook (vd. 'giuw' -> 'giư', đứng đầu chuỗi 'giường').
        if (d.base == L'u' && j > 0) {
            auto p = CayData::DecomposeChar(_text[j-1]);
            bool isGiCluster = (p.base == L'i' && j >= 2
                                && CayData::ToLowerViet(_text[j-2]) == L'g');
            if ((CayData::IsVowel(p.base) && !isGiCluster) || p.base == L'q') {
                continue;
            }
        }

        // 2d. Default hook rule (oo→ô, aw→ă, ow→ơ, uw→ư, â→ă, ...).
        wchar_t hookRule = CayData::GetHookRule(d.base);
        if (hookRule != L'\0') {
            _text[j] = CayData::ComposeChar(hookRule, d.toneIndex, d.isUpper);
            return true;
        }

        if (!CayData::IsVowel(d.base)) {
            continue;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// ApplyToneMarks
//
// Applies or changes the tone mark on the appropriate vowel of _text[].
// If the same tone is applied twice, removes it (toggle).
// ---------------------------------------------------------------------------
bool TelexEngine::ApplyToneMarks(int toneIndex) {
    if (_textLen == 0) return false;

    // --- SMART BYPASS: Từ chối bỏ dấu nếu phát hiện các nguyên âm rời rạc ---
    // Ví dụ "remix", có 'e' và 'i' cách nhau bởi 'm'. Điều này không tồn tại trong 1 âm tiết tiếng Việt.
    int firstVowel = -1, lastVowel = -1, vowelCount = 0;
    for (int i = 0; i < _textLen; i++) {
        if (CayData::IsVowel(_text[i])) {
            if (firstVowel == -1) firstVowel = i;
            lastVowel = i;
            vowelCount++;
        }
    }
    if (vowelCount > 1 && (lastVowel - firstVowel >= vowelCount)) {
        return false; // Bỏ qua, nhả lại ký tự thô (x, s, f, r, j, z)
    }
    // ------------------------------------------------------------------------

    int currentTone = 0;
    int tonePos = -1;

    // 1. Identify if the word currently has a tone and where it is.
    //    DecomposeChar trả về (base, toneIndex, isUpper) trong O(1) — thay block
    //    quét GetToneMark qua vòng lặp t=1..5 (Requirement 4.6/4.7).
    for (int i = 0; i < _textLen; i++) {
        auto d = CayData::DecomposeChar(_text[i]);
        if (d.toneIndex > 0) {
            currentTone = d.toneIndex;
            tonePos = i;
            break;
        }
    }

    // 2. Handle 'z' key (toneIndex == 0)
    if (toneIndex == 0) {
        if (currentTone > 0) {
            // Strip dấu thanh giữ uppercase qua ComposeChar(base, 0, isUpper).
            auto d = CayData::DecomposeChar(_text[tonePos]);
            _text[tonePos] = CayData::ComposeChar(d.base, 0, d.isUpper);
            _toneIndex = -1;

            return true; // Consumed 'z', do not append it
        }
        return false; // No tone to remove, return false so 'z' gets appended normally
    }

    // 3. Handle double-typing the SAME tone key (Undo tone and append raw key)
    if (currentTone == toneIndex) {
        auto d = CayData::DecomposeChar(_text[tonePos]);
        _text[tonePos] = CayData::ComposeChar(d.base, 0, d.isUpper);
        _toneIndex = -1;

        return false; // Return false so the raw tone key (e.g., 's') gets appended
    }

    // --- TÍNH NĂNG 2: AUTO HOOK UO -> ƯƠ ---
    // Chỉ kích hoạt nếu user bấm dấu thanh (s, f, r, x, j) và không phải là xóa dấu (z)
    if (toneIndex > 0) {
        for (int i = 0; i < _textLen - 1; i++) {
            auto d1 = CayData::DecomposeChar(_text[i]);
            auto d2 = CayData::DecomposeChar(_text[i+1]);

            // Nếu phát hiện cặp "uo" đi liền nhau (đã strip tone qua DecomposeChar.base)
            if (d1.base == L'u' && d2.base == L'o') {
                // Ngoại lệ: Nếu trước 'u' là 'q' (vd: "quốc"), thì KHÔNG được móc thành "qước"
                bool isQu = (i > 0 && CayData::ToLowerViet(_text[i-1]) == L'q');
                if (!isQu) {
                    // Ép buộc u -> ư và o -> ơ in place; drop tone hiện tại để
                    // bước 4 đặt lại đúng vị trí (tonePos sẽ được reset).
                    _text[i]   = CayData::ComposeChar(L'\u01b0', 0, d1.isUpper); // ư / Ư
                    _text[i+1] = CayData::ComposeChar(L'\u01a1', 0, d2.isUpper); // ơ / Ơ

                    // Nếu chữ đã có dấu thanh từ trước, cần reset lại tonePos vì chữ cái gốc đã bị thay đổi
                    tonePos = -1;
                }
                break;
            }
        }
    }
    // ---------------------------------------

    // 4. Apply or replace tone
    int targetPos = (tonePos >= 0) ? tonePos : FindTonePosition();
    if (targetPos >= 0) {
        auto d = CayData::DecomposeChar(_text[targetPos]);

        // ComposeChar trả về 0 nếu (base, toneIndex) không có mapping → giữ nguyên hành vi cũ.
        wchar_t toned = CayData::ComposeChar(d.base, toneIndex, d.isUpper);
        if (toned != L'\0') {
            _text[targetPos] = toned;
            _toneIndex = toneIndex;

            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// OnKeyDown – main entry point
// ---------------------------------------------------------------------------
void TelexEngine::OnKeyDown(Cay::KeyEvent& e) {
    // 0. Reset state on Navigation or Control keys to prevent buffer desync
    if (e.keyCode == Cay::KeyCode::Escape ||
       (e.keyCode >= Cay::KeyCode::PageUp && e.keyCode <= Cay::KeyCode::Down)) { 
        // Cay::KeyCode::PageUp (33) to Cay::KeyCode::Down (40) covers PageUp, PageDown, End, Home, Left, Up, Right, Down
        ResetState();
        _canRestore = false;
        return;
    }

    Cay::KeyCode vk = e.keyCode;

    // -----------------------------------------------------------------------
    // 1. Non-alpha keys that reset or terminate the current word.
    // -----------------------------------------------------------------------
    switch (vk) {
    case Cay::KeyCode::Backspace:
        if (_bufferCount == 0 && _canRestore) {
            // Restore state
            _bufferCount = _savedBufferCount;
            for (int i = 0; i < _bufferCount; i++) _buffer[i] = _savedBuffer[i];
            
            _textLen = _savedTextLen;
            for (int i = 0; i < _textLen; i++) _text[i] = _savedText[i];
            
            _toneIndex = _savedToneIndex;
            
            // Sync output state with restored text (so UpdateScreen works properly on next keystroke)
            _lastOutputLen = _savedTextLen;
            for (int i = 0; i < _savedTextLen; i++) _lastOutput[i] = _savedText[i];
            _lastOutput[_lastOutputLen] = L'\0';
            
            _canRestore = false;
            // DO NOT set e.handled = true here. We let the OS physically delete the Space character.
            return;
        }

        if (_bufferCount > 0) {
            e.handled = true; // suppress the raw backspace
            _canRestore = false;

            // Save visual output state
            int savedLastOutputLen = _lastOutputLen;
            wchar_t savedLastOutput[MAX_BUFFER];
            for (int i = 0; i < savedLastOutputLen; i++) savedLastOutput[i] = _lastOutput[i];

            MyKey tempBuffer[MAX_BUFFER];
            int tempCount = _bufferCount;
            for (int i = 0; i < tempCount; i++) tempBuffer[i] = _buffer[i];

            // --- DRY RUN: replay all keys, tracking dependencies ---
            int createsIndex[MAX_BUFFER];       // which visual index each key created (-1 if none)
            int numModified[MAX_BUFFER];         // how many visual indices each key modified
            bool touchesTarget[MAX_BUFFER];      // does key i modify the LAST visual index?

            ResetState();

            for (int i = 0; i < tempCount; i++) {
                createsIndex[i] = -1;
                numModified[i] = 0;
                touchesTarget[i] = false;

                int oldTextLen = _textLen;
                wchar_t oldText[MAX_BUFFER];
                for (int j = 0; j < oldTextLen; j++) oldText[j] = _text[j];

                ReplayKey(tempBuffer[i].raw);

                // Hybrid tracking: detect all changes
                int target = _textLen - 1;  // last visual index after this key
                for (int j = 0; j < _textLen && j < MAX_BUFFER; j++) {
                    if (j >= oldTextLen || _text[j] != oldText[j]) {
                        numModified[i]++;
                    }
                }
                if (_textLen > oldTextLen) {
                    createsIndex[i] = _textLen - 1;
                    // Creation is not a modification — subtract if counted
                    if (numModified[i] > 0) numModified[i]--;
                }
            }

            // Re-check: which keys touch the FINAL visual index?
            int targetVisualIndex = _textLen - 1;
            if (targetVisualIndex >= 0) {
                // We need to know which keys modified targetVisualIndex.
                // Re-run a quick check: replay again, this time only tracking target.
                ResetState();
                for (int i = 0; i < tempCount; i++) {
                    int oldTextLen = _textLen;
                    wchar_t oldChar = (targetVisualIndex < _textLen) ? _text[targetVisualIndex] : 0;

                    ReplayKey(tempBuffer[i].raw);

                    wchar_t newChar = (targetVisualIndex < _textLen) ? _text[targetVisualIndex] : 0;
                    if (oldChar != newChar && createsIndex[i] != targetVisualIndex) {
                        touchesTarget[i] = true;
                    }
                }
            }

            // --- FIND KEYS TO DELETE ---
            bool deleteKey[MAX_BUFFER] = {false};
            bool foundAny = false;

            if (targetVisualIndex >= 0) {
                for (int i = 0; i < tempCount; i++) {
                    if (createsIndex[i] == targetVisualIndex) {
                        deleteKey[i] = true;
                        foundAny = true;
                    }
                    if (touchesTarget[i] && numModified[i] <= 1) {
                        deleteKey[i] = true;
                        foundAny = true;
                    }
                }
            }
            if (!foundAny && tempCount > 0) {
                deleteKey[tempCount - 1] = true;
            }

            // --- FILTER + ACTUAL REPLAY ---
            ResetState();
            for (int i = 0; i < tempCount; i++) {
                if (!deleteKey[i]) ReplayKey(tempBuffer[i].raw);
            }

            // Restore visual output state
            _lastOutputLen = savedLastOutputLen;
            for (int i = 0; i < savedLastOutputLen; i++) _lastOutput[i] = savedLastOutput[i];
            _lastOutput[_lastOutputLen] = L'\0';

            UpdateScreen(_text, _textLen);
        }
        return;

    case Cay::KeyCode::Escape:
        ResetFull();
        return;

    case Cay::KeyCode::Enter:
    case Cay::KeyCode::Tab:
    case Cay::KeyCode::Space:
        if (_bufferCount > 0) {
            // --- TÍNH NĂNG 1: RESTORE ON SPACE ---
            // Kiểm tra xem từ hiện tại có dấu tiếng Việt không
            bool hasVietMark = CayData::HasVietnameseMark(_text, _textLen);
            if (hasVietMark) {
                // Trích xuất chuỗi chữ cái cơ bản (đã lột sạch dấu thanh và dấu mũ)
                wchar_t textLo[MAX_BUFFER];
                for (int i = 0; i < _textLen; i++) {
                    textLo[i] = CayData::ToLowerViet(CayData::StripTone(_text[i])); // Giữ lại cấu trúc thuần
                }
                textLo[_textLen] = L'\0';
                
                // Đưa vào máy quét cấu trúc âm tiết. Nếu là từ vô nghĩa (như "vietlott")...
                if (!IsCompleteSyllable(textLo, _textLen)) {
                    FallbackToRaw(); // ...Lập tức xóa từ có dấu trên màn hình, nhả lại text thô!
                }
            }
            // -------------------------------------
            CommitWord();
        }
        else ResetFull();
        return;

    case Cay::KeyCode::Left: case Cay::KeyCode::Right: case Cay::KeyCode::Up: case Cay::KeyCode::Down:
    case Cay::KeyCode::Home: case Cay::KeyCode::End:  case Cay::KeyCode::PageUp: case Cay::KeyCode::PageDown:
    case Cay::KeyCode::Delete:
        ResetFull();
        return;
    default:
        break;
    }

    // -----------------------------------------------------------------------
    // 2. Only process printable ASCII alpha characters.
    // -----------------------------------------------------------------------
    if (vk < Cay::KeyCode::KeyA || vk > Cay::KeyCode::KeyZ) {
        // Non-alpha printable (digits, punctuation) – commit word.
        if (_bufferCount > 0) CommitWord();
        else ResetFull();
        return;
    }

    // Determine the actual character pressed (respecting Shift).
    wchar_t ch = e.character;
    wchar_t lo = CayData::ToLowerViet(ch);

    // -----------------------------------------------------------------------
    // 3. Guard: buffer overflow -> fall through as plain text.
    // -----------------------------------------------------------------------
    if (_bufferCount >= MAX_BUFFER - 1 || _textLen >= MAX_BUFFER - 1) {
        ResetFull();
        // Do NOT set e.handled = true here, let the OS handle the keystroke naturally
        return;
    }

    // -----------------------------------------------------------------------
    // 4. Record raw keystroke.
    // -----------------------------------------------------------------------
    if (_bufferCount == 0) _canRestore = false;

    // -----------------------------------------------------------------------
    // 4–6. Record + Try modifiers + Fallback append (single function).
    // -----------------------------------------------------------------------
    ReplayKey(ch);

    // -----------------------------------------------------------------------
    // 7. Sync screen (Centralized Rendering)
    // -----------------------------------------------------------------------
    e.handled = true;
    UpdateScreen(_text, _textLen);
}

// ---------------------------------------------------------------------------
// OnKeyUp – currently unused; reserved for future modifier tracking.
// ---------------------------------------------------------------------------
void TelexEngine::OnKeyUp(Cay::KeyEvent& e) {
    // No-op for now.
    (void)e;
}

#ifdef CAY_TEST_BUILD
// ---------------------------------------------------------------------------
// DebugSetText (test-only)
//
// Set _text[] trực tiếp từ test code để cô lập các const private method
// (FindTonePosition, ShouldBypassWord, ...) khỏi chuỗi keystroke.
// Truncate đến MAX_BUFFER - 1 để dành chỗ null terminator.
// ---------------------------------------------------------------------------
void TelexEngine::DebugSetText(const wchar_t* s, int len) {
    if (len < 0) len = 0;
    if (len > MAX_BUFFER - 1) len = MAX_BUFFER - 1;
    for (int i = 0; i < len; i++) _text[i] = s[i];
    _text[len] = L'\0';
    _textLen = len;
}

// ---------------------------------------------------------------------------
// GetDebugState (test-only)
//
// Snapshot toàn bộ private state vào struct DebugState để Property 11 / 12
// (test/test_idempotency.cpp) so sánh state trước-sau khi gọi reset operation.
// Copy field-by-field — không dùng memcpy vì layout của TelexEngine không
// đảm bảo tương đương DebugState (chỉ tương đương ngữ nghĩa).
// ---------------------------------------------------------------------------
DebugState TelexEngine::GetDebugState() const {
    DebugState s{};
    for (int i = 0; i < MAX_BUFFER; i++) {
        s.buffer[i]      = _buffer[i];
        s.text[i]        = _text[i];
        s.lastOutput[i]  = _lastOutput[i];
        s.savedBuffer[i] = _savedBuffer[i];
        s.savedText[i]   = _savedText[i];
    }
    s.bufferCount      = _bufferCount;
    s.textLen          = _textLen;
    s.toneIndex        = _toneIndex;
    s.lastOutputLen    = _lastOutputLen;
    s.savedBufferCount = _savedBufferCount;
    s.savedTextLen     = _savedTextLen;
    s.savedToneIndex   = _savedToneIndex;
    s.canRestore       = _canRestore;
    return s;
}
#endif

} // namespace Cay





