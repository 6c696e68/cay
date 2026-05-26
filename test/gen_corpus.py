#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generator cho test/corpus.h — sinh corpus C++ header từ danh sách
Vietnamese words (Unicode) và English words (ASCII).

Chạy:
    python3 test/gen_corpus.py > test/corpus.h

Phục vụ task 1.3 của spec engine-core-refactor:
    - Property 9 (FindTonePosition) — Requirements 7.1..7.8
    - Property 10 (ShouldBypassWord) — Requirements 8.1..8.7
    - Property 16 (IsValidNucleus)   — Requirements 3.4..3.6

Convention:
    - Mọi ký tự Unicode trong wchar_t literal dùng \\uXXXX escape (design.md).
    - File header-only, không include STL.
"""
import sys
import unicodedata

# Combining marks tiếng Việt (NFD form): huyền, sắc, hỏi, ngã, nặng.
_TONE_COMBINING = {
    '\u0300': 1,  # huyền (grave)
    '\u0301': 2,  # sắc (acute)
    '\u0309': 3,  # hỏi (hook above)
    '\u0303': 4,  # ngã (tilde)
    '\u0323': 5,  # nặng (dot below)
}

# Telex modifier key cho mỗi tone index.
_TONE_KEY = {0: 'z', 1: 'f', 2: 's', 3: 'r', 4: 'x', 5: 'j'}

def has_tone(ch):
    """True nếu ch (Vietnamese precomposed) mang dấu thanh."""
    nfd = unicodedata.normalize('NFD', ch)
    return any(c in _TONE_COMBINING for c in nfd)

def find_tone_pos(s):
    """Trả về index của ký tự đầu tiên trong s mang dấu thanh; -1 nếu không có."""
    for i, c in enumerate(s):
        if has_tone(c):
            return i
    return -1

def to_wchar_escape(s):
    """Convert string sang dạng L"..." với \\uXXXX escape cho mọi non-ASCII."""
    out = []
    for c in s:
        cp = ord(c)
        if cp < 0x80:
            if c == '\\':
                out.append('\\\\')
            elif c == '"':
                out.append('\\"')
            else:
                out.append(c)
        elif cp <= 0xFFFF:
            out.append(f'\\u{cp:04X}')
        else:
            out.append(f'\\U{cp:08X}')
    return f'L"{"".join(out)}"'

def reverse_telex(viet_word):
    """
    Sinh chuỗi phím Telex (ASCII) từ một từ tiếng Việt Unicode.
    Quy ước Telex chuẩn:
        - Tone: huyền=f, sắc=s, hỏi=r, ngã=x, nặng=j (gõ sau cùng từ)
        - Hook/circumflex: aa=â, ee=ê, oo=ô, dd=đ, aw=ă, ow=ơ, uw=ư
    """
    # 1. NFD-decompose toàn bộ từ để tách tone combining marks.
    nfd = unicodedata.normalize('NFD', viet_word)
    tone_index = 0
    no_tone_chars = []
    for c in nfd:
        if c in _TONE_COMBINING:
            tone_index = _TONE_COMBINING[c]
        else:
            no_tone_chars.append(c)
    # 2. Re-compose phần không-có-tone về NFC để có precomposed â/ă/ê/ô/ơ/ư/đ.
    no_tone = unicodedata.normalize('NFC', ''.join(no_tone_chars))
    # 3. Convert từng ký tự precomposed sang Telex sequence.
    out = []
    for ch in no_tone:
        out.append(_base_to_telex(ch))
    # 4. Append tone key (Telex chuẩn: đặt sau từ).
    if tone_index > 0:
        out.append(_TONE_KEY[tone_index])
    return ''.join(out)

# Map nguyên âm/phụ âm có dấu mũ/móc (đã strip tone) sang Telex sequence.
_BASE_TO_TELEX = {
    'â': 'aa', 'Â': 'AA',
    'ă': 'aw', 'Ă': 'AW',
    'ê': 'ee', 'Ê': 'EE',
    'ô': 'oo', 'Ô': 'OO',
    'ơ': 'ow', 'Ơ': 'OW',
    'ư': 'uw', 'Ư': 'UW',
    'đ': 'dd', 'Đ': 'DD',
}

def _base_to_telex(base):
    """Convert ký tự base (đã strip tone) sang chuỗi Telex."""
    return _BASE_TO_TELEX.get(base, base)

# ---------------------------------------------------------------------------
# Vietnamese corpus — 200 từ.
# Mỗi tuple: (vietnamese_unicode, expected_nucleus).
# telex_input được gen tự động bằng reverse_telex().
# expected_tone_pos được gen tự động bằng find_tone_pos().
# ---------------------------------------------------------------------------

# GROUP A — 100 từ phổ thông (high-frequency)
group_a = [
    # A1. Người và gia đình (15)
    ("tôi",   "ôi"),
    ("bạn",   "a"),
    ("anh",   "a"),
    ("chị",   "i"),
    ("em",    "e"),
    ("cha",   "a"),
    ("mẹ",    "e"),
    ("con",   "o"),
    ("ông",   "ô"),
    ("bà",    "a"),
    ("chú",   "u"),
    ("vợ",    "ơ"),
    ("chồng", "ô"),
    ("bận",   "â"),
    ("đứa",   "ưa"),

    # A2. Nhà và môi trường (10)
    ("nhà",   "a"),
    ("nước",  "ươ"),
    ("đất",   "â"),
    ("trời",  "ơi"),
    ("sông",  "ô"),
    ("suối",  "uô"),
    ("núi",   "ui"),
    ("biển",  "iê"),
    ("rừng",  "ư"),
    ("đồng",  "ô"),

    # A3. Thời tiết (10)
    ("gió",   "o"),
    ("nắng",  "ă"),
    ("mưa",   "ưa"),
    ("tuyết", "uyê"),
    ("bão",   "ao"),
    ("sương", "ươ"),
    ("mây",   "ây"),
    ("sấm",   "â"),
    ("băng",  "ă"),
    ("nóng",  "o"),

    # A4. Thực vật (10)
    ("hoa",   "oa"),
    ("lá",    "a"),
    ("cây",   "ây"),
    ("cỏ",    "o"),
    ("sen",   "e"),
    ("quả",   "a"),
    ("bưởi",  "ươi"),
    ("chuối", "uô"),
    ("đào",   "ao"),
    ("nho",   "o"),

    # A5. Động vật (15)
    ("chim",  "i"),
    ("cá",    "a"),
    ("vịt",   "i"),
    ("gà",    "a"),
    ("lợn",   "ơ"),
    ("bò",    "o"),
    ("ngựa",  "ưa"),
    ("chó",   "o"),
    ("mèo",   "eo"),
    ("chuột", "uô"),
    ("rồng",  "ô"),
    ("voi",   "oi"),
    ("hổ",    "ô"),
    ("gấu",   "âu"),
    ("khỉ",   "i"),

    # A6. Động vật tiếp (10)
    ("sóc",   "o"),
    ("thỏ",   "o"),
    ("rắn",   "ă"),
    ("ếch",   "ê"),
    ("ốc",    "ô"),
    ("sáo",   "ao"),
    ("dê",    "ê"),
    ("trâu",  "âu"),
    ("bướm",  "ươ"),
    ("ong",   "o"),

    # A7. Đồ ăn (10)
    ("cơm",   "ơ"),
    ("bánh",  "a"),
    ("phở",   "ơ"),
    ("bún",   "u"),
    ("mì",    "i"),
    ("thịt",  "i"),
    ("cá",    "a"),  # duplicate "cá" — cần thay
    ("xôi",   "ôi"),
    ("rượu",  "ươ"),
    ("trà",   "a"),

    # A8. Màu sắc và tính chất (15)
    ("đỏ",    "o"),
    ("vàng",  "a"),
    ("xanh",  "a"),
    ("tím",   "i"),
    ("trắng", "ă"),
    ("đen",   "e"),
    ("lớn",   "ơ"),
    ("nhỏ",   "o"),
    ("cao",   "ao"),
    ("thấp",  "â"),
    ("dài",   "ai"),
    ("ngắn",  "ă"),
    ("nhanh", "a"),
    ("chậm",  "â"),
    ("đẹp",   "e"),

    # A9. Hành động (15)
    ("ăn",    "ă"),
    ("uống",  "uô"),
    ("ngủ",   "u"),
    ("đi",    "i"),
    ("chạy",  "ay"),
    ("nói",   "oi"),
    ("nghe",  "e"),
    ("học",   "o"),
    ("viết",  "iê"),
    ("đọc",   "o"),
    ("làm",   "a"),
    ("chơi",  "ơi"),
    ("hát",   "a"),
    ("múa",   "ua"),
    ("yêu",   "yêu"),
]

# Loại bỏ duplicate "cá" — thay bằng "tôm".
# Tìm và replace duplicate.
_seen_a = set()
group_a_clean = []
for v, n in group_a:
    if v in _seen_a:
        continue  # bỏ duplicate
    _seen_a.add(v)
    group_a_clean.append((v, n))
# Bù vào nếu thiếu.
_extra_a = [
    ("tôm",   "ô"),
    ("cua",   "ua"),
    ("nai",   "ai"),
]
while len(group_a_clean) < 100:
    for x in _extra_a:
        if x[0] not in _seen_a:
            group_a_clean.append(x)
            _seen_a.add(x[0])
            if len(group_a_clean) >= 100:
                break
    break  # tránh vòng vô tận
group_a = group_a_clean[:100]

# GROUP B — 50 tổ hợp dấu khó
group_b = [
    # B1. mơ/mờ/mở/mỡ/mớ/mợ (6)
    ("mơ",    "ơ"),
    ("mờ",    "ơ"),
    ("mớ",    "ơ"),
    ("mở",    "ơ"),
    ("mỡ",    "ơ"),
    ("mợ",    "ơ"),

    # B2. ma/mà/má/mả/mã/mạ (6)
    ("ma",    "a"),
    ("mà",    "a"),
    ("má",    "a"),
    ("mả",    "a"),
    ("mã",    "a"),
    ("mạ",    "a"),

    # B3. Tổ hợp 3 nguyên âm khó (10)
    ("hươu",     "ươu"),
    ("nguyễn",   "uyê"),
    ("giường",   "ươ"),
    ("trường",   "ươ"),
    ("đường",    "ươ"),
    ("mượn",     "ươ"),
    ("xướng",    "ươ"),
    ("tượng",    "ươ"),
    ("phương",   "ươ"),
    ("thương",   "ươ"),

    # B4. uyê / uya / quy / qua / quý (10)
    ("khuyết",   "uyê"),
    ("tuyệt",    "uyê"),
    ("nguyên",   "uyê"),
    ("chuyền",   "uyê"),
    ("quý",      "y"),
    ("quà",      "a"),
    ("quây",     "ây"),
    ("khuya",    "uya"),
    # Cặp mở "uy" không có phụ âm cuối → dấu thanh đặt trên nguyên âm 2 (y)
    # theo Requirement 7.3 (FindTonePosition: oa/oe/uê/uy/uơ/iê → last vowel).
    ("thuỳ",     "uy"),
    ("quốc",     "uô"),

    # B5. iê / yê đa dạng (10)
    ("yêu",      "yêu"),
    ("kiềm",     "iê"),
    ("tiền",     "iê"),
    ("chiến",    "iê"),
    ("diện",     "iê"),
    ("hiện",     "iê"),
    ("tiếng",    "iê"),
    ("biếng",    "iê"),
    ("yến",      "yê"),
    ("triều",    "iêu"),

    # B6. d/đ và w combos (8)
    ("đống",     "ô"),
    ("đưa",      "ưa"),
    ("đài",      "ai"),
    ("đứng",     "ư"),
    ("đỉnh",     "i"),
    ("đảng",     "a"),
    ("đợi",      "ơi"),
    ("đoàn",     "oa"),
]
group_b = group_b[:50]

# GROUP C — 50 từ test FindTonePosition
group_c = [
    # C1. 1 nguyên âm + tone (10)
    ("cá",     "a"),
    ("cà",     "a"),
    ("cả",     "a"),
    ("cạ",     "a"),
    ("cã",     "a"),
    ("ề",      "ê"),
    ("ồ",      "ô"),
    ("ó",      "o"),
    ("ú",      "u"),
    ("ỳ",      "y"),

    # C2. 2 nguyên âm + final → tone on 2nd vowel (10)
    ("tuấn",   "uâ"),
    ("điện",   "iê"),
    ("hoàn",   "oa"),
    ("toán",   "oa"),
    ("xoán",   "oa"),
    ("luận",   "uâ"),
    ("muộn",   "uô"),
    ("buộc",   "uô"),
    ("nguồn",  "uô"),
    ("vượn",   "ươ"),

    # C3. 2 nguyên âm mở thuộc {oa,oe,uê,uy,iê} → tone on 2nd (8)
    # Requirement 7.3: với cặp mở (không có phụ âm cuối) {oa,oe,uê,uy,uơ,iê},
    # FindTonePosition đặt dấu thanh trên nguyên âm thứ HAI (last), không phải nguyên âm đầu.
    ("hoà",    "oa"),
    ("oè",     "oe"),
    ("thuý",   "uy"),
    ("quế",    "ê"),    # qu+ê, sắc trên ê
    ("viễ",    "iê"),
    ("hoẹ",    "oe"),
    ("uể",     "uê"),
    ("quỳ",    "y"),    # qu+y, huyền trên y

    # C4. 2 nguyên âm mở khác → tone on 1st (8)
    ("rồi",    "ôi"),
    ("mèo",    "eo"),
    ("đồi",    "ôi"),
    ("bơi",    "ơi"),
    ("múa",    "ua"),
    ("mùa",    "ua"),
    ("bài",    "ai"),
    ("túi",    "ui"),

    # C5. 3 nguyên âm + final → tone on last (5)
    ("tuyến",   "uyê"),
    ("giường",  "ươ"),
    ("ngoằn",   "oa"),
    ("xướng",   "ươ"),
    ("nguyễn",  "uyê"),

    # C6. 3 nguyên âm mở → tone on middle (5)
    ("ngoài",   "oai"),
    ("người",   "ươi"),
    ("bưởi",    "ươi"),
    ("rượi",    "ươi"),
    ("tuổi",    "uô"),

    # C7. qu/gi prefix (4)
    ("già",     "a"),
    ("giáo",    "ao"),
    ("giả",     "a"),
    ("giúp",    "u"),
]
group_c = group_c[:50]

# Loại duplicate giữa các nhóm.
all_entries = []
seen = set()
for grp in (group_a, group_b, group_c):
    for v, n in grp:
        if v in seen:
            continue
        seen.add(v)
        all_entries.append((v, n))

# Bù thêm nếu thiếu để đủ 200.
_extra_pad = [
    ("đêm",    "ê"),
    ("sáng",   "a"),
    ("chiều",  "iê"),
    ("trưa",   "ưa"),
    ("hôm",    "ô"),
    ("nay",    "ay"),
    ("mai",    "ai"),
    ("tuần",   "uâ"),
    ("tháng",  "a"),
    ("năm",    "ă"),
    ("xe",     "e"),
    ("tàu",    "au"),
    ("máy",    "ay"),
    ("bay",    "ay"),
    ("đường",  "ươ"),  # may already exist
    ("phố",    "ô"),
    ("làng",   "a"),
    ("quê",    "ê"),
    ("trường", "ươ"),
    ("lớp",    "ơ"),
    ("sách",   "a"),
    ("vở",     "ơ"),
    ("bút",    "u"),
    ("mực",    "ư"),
    ("giấy",   "ây"),
    ("bàn",    "a"),
    ("ghế",    "ê"),
    ("đèn",    "e"),
    ("quạt",   "a"),
    ("máy",    "ay"),  # may already exist
    ("tủ",     "u"),
    ("giường", "ươ"),  # may already exist
    ("chăn",   "ă"),
    ("gối",    "ô"),
    ("nệm",    "ê"),
    ("cốc",    "ô"),
    ("bát",    "a"),
    ("đĩa",    "i"),
    ("chén",   "e"),
    ("nồi",    "ôi"),
    ("chảo",   "ao"),
    ("dao",    "ao"),
    ("kéo",    "eo"),
    ("búa",    "ua"),
    ("cuốc",   "uô"),
    ("xẻng",   "e"),
    ("cuốn",   "uô"),
    ("tập",    "â"),
    ("báo",    "ao"),
    ("thư",    "ư"),
    ("phim",   "i"),
    ("kịch",   "i"),
    ("nhạc",   "a"),
    ("đàn",    "a"),
    ("trống",  "ô"),
    ("sáo",    "ao"),  # may already exist
    ("kèn",    "e"),
]
for x in _extra_pad:
    if len(all_entries) >= 200:
        break
    if x[0] not in seen:
        all_entries.append(x)
        seen.add(x[0])

if len(all_entries) > 200:
    all_entries = all_entries[:200]

if len(all_entries) != 200:
    sys.stderr.write(f"FATAL: Vietnamese entries = {len(all_entries)}, need 200\n")
    sys.exit(1)

# ---------------------------------------------------------------------------
# English corpus — 50 entries (Requirement 8 bypass coverage).
# ---------------------------------------------------------------------------
eng_entries = [
    # Bypass: bắt đầu w/f/j/z (10) — Requirement 8.1
    "write", "work", "world", "want",
    "fast", "food", "free", "flag",
    "just", "jump",

    # Bypass: q + non-u (5) — Requirement 8.2
    "qmail", "qatar", "qed", "qrs", "qwerty",

    # Bypass: p + non-h (10) — Requirement 8.3
    "public", "project", "plan", "place", "post",
    "pro", "pop", "pin", "pay", "pen",

    # Bypass: cluster đầu không thuộc 8 cụm Việt hợp lệ (15) — Requirement 8.4
    "class", "style", "block", "brick", "drink",
    "smell", "stack", "swing", "sleep", "click",
    "dream", "cry", "fly", "glad", "scan",

    # Bypass khác — w/z/structural (10)
    "zoom", "zero", "will", "when",
    "width", "height", "function", "import", "output", "input",
]
assert len(eng_entries) == 50

# ---------------------------------------------------------------------------
# Generate corpus.h
# ---------------------------------------------------------------------------

def main():
    out = []
    out.append("// =============================================================================")
    out.append("// test/corpus.h — Test corpus cho engine-core-refactor")
    out.append("//")
    out.append("// Auto-generated bởi test/gen_corpus.py — KHÔNG sửa file này trực tiếp.")
    out.append("// Sửa danh sách entries trong gen_corpus.py rồi chạy lại generator.")
    out.append("//")
    out.append("// Phục vụ:")
    out.append("//   - Property 9  (FindTonePosition) — Requirements 7.1..7.8")
    out.append("//   - Property 10 (ShouldBypassWord) — Requirements 8.1..8.7")
    out.append("//   - Property 16 (IsValidNucleus)   — Requirements 3.4..3.6")
    out.append("//")
    out.append("// Convention: mọi ký tự Unicode dùng \\uXXXX escape (design.md Testing Strategy).")
    out.append("// Header-only, không include STL — phù hợp với test build.")
    out.append("// =============================================================================")
    out.append("#ifndef CAY_TEST_CORPUS_H")
    out.append("#define CAY_TEST_CORPUS_H")
    out.append("")
    out.append("namespace CayTestCorpus {")
    out.append("")
    out.append("// ---------------------------------------------------------------------------")
    out.append("// VietnameseEntry: 1 entry corpus tiếng Việt với annotation phục vụ test.")
    out.append("//   - telexInput      : chuỗi phím người dùng gõ (ASCII).")
    out.append("//   - expectedOutput  : chuỗi Unicode kỳ vọng sau khi engine xử lý.")
    out.append("//   - expectedNucleus : phần nucleus (đã strip dấu thanh, lowercase,")
    out.append("//                       giữ dấu mũ/móc — vd \"trường\" → \"ươ\").")
    out.append("//   - expectedTonePos : index trong expectedOutput của ký tự mang dấu thanh,")
    out.append("//                       hoặc -1 nếu không có dấu thanh.")
    out.append("// ---------------------------------------------------------------------------")
    out.append("struct VietnameseEntry {")
    out.append("    const wchar_t* telexInput;")
    out.append("    const wchar_t* expectedOutput;")
    out.append("    const wchar_t* expectedNucleus;")
    out.append("    int            expectedTonePos;")
    out.append("};")
    out.append("")
    out.append("// ---------------------------------------------------------------------------")
    out.append("// EnglishEntry: 1 từ tiếng Anh kỳ vọng được bypass (ShouldBypassWord==true).")
    out.append("// ---------------------------------------------------------------------------")
    out.append("struct EnglishEntry {")
    out.append("    const char* word;")
    out.append("};")
    out.append("")
    out.append("// ---------------------------------------------------------------------------")
    out.append("// 200 từ tiếng Việt — coverage:")
    out.append("//   - Group A: 100 từ phổ thông tần suất cao")
    out.append("//   - Group B: 50  tổ hợp dấu khó (mơ/mờ/mở/mỡ, hươu, nguyễn, giường, ...)")
    out.append("//   - Group C: 50  từ test FindTonePosition (1/2/3 nguyên âm, có/không cuối)")
    out.append("// ---------------------------------------------------------------------------")
    out.append("static const VietnameseEntry g_vietnameseCorpus[200] = {")

    for viet, nucleus in all_entries:
        telex = reverse_telex(viet)
        tone_pos = find_tone_pos(viet)
        line = "    { %s, %s, %s, %d }," % (
            to_wchar_escape(telex),
            to_wchar_escape(viet),
            to_wchar_escape(nucleus),
            tone_pos,
        )
        out.append(line)

    out.append("};")
    out.append("")
    out.append("static constexpr int g_vietnameseCorpusCount = 200;")
    out.append("")
    out.append("// ---------------------------------------------------------------------------")
    out.append("// 50 từ tiếng Anh — coverage cho Requirement 8 bypass rules:")
    out.append("//   - 10 từ bắt đầu w/f/j (R8.1)")
    out.append("//   - 5  từ q + non-u    (R8.2)")
    out.append("//   - 10 từ p + non-h    (R8.3)")
    out.append("//   - 15 từ cluster đầu không Việt (R8.4)")
    out.append("//   - 10 từ z/w/structural khác")
    out.append("// ---------------------------------------------------------------------------")
    out.append("static const EnglishEntry g_englishCorpus[50] = {")

    for word in eng_entries:
        out.append('    { "%s" },' % word)

    out.append("};")
    out.append("")
    out.append("static constexpr int g_englishCorpusCount = 50;")
    out.append("")
    out.append("} // namespace CayTestCorpus")
    out.append("")
    out.append("#endif // CAY_TEST_CORPUS_H")
    out.append("")

    print("\n".join(out))

if __name__ == "__main__":
    main()
