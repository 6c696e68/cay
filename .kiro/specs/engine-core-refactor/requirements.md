# Requirements Document

## Introduction

Tài liệu này đặc tả yêu cầu cho đợt refactor engine core của Cay — bộ gõ Telex tiếng Việt viết bằng C++ thuần, đa nền tảng (Windows no-CRT, macOS bundle, Linux Fcitx5). Phạm vi refactor giới hạn trong `src/core/` (CayData, CayEngine, CayTypes) và những điểm tích hợp tối thiểu ở platform layer khi cần (kích thước buffer, hardcoded list).

Mục tiêu của refactor là cải thiện chất lượng nội tại của engine core mà KHÔNG thay đổi hành vi từ góc nhìn người dùng cuối: cùng một chuỗi phím Telex phải sinh ra cùng một chuỗi Unicode tiếng Việt như phiên bản trước refactor. Refactor cần loại bỏ dead code, dữ liệu sai (bảng nucleus có rác, duplicate), code lặp (logic decompose nguyên âm + tone + uppercase được copy >10 lần), single source of truth bị vi phạm (IsCompleteSyllable định nghĩa lại tables cục bộ thay vì dùng CayData), magic number không nhất quán (MAX_BUFFER 64 vs chars[64] vs inputs[256]), và mojibake trong comments.

Mọi ràng buộc phi chức năng của dự án phải được giữ nguyên: no-CRT trên Windows (không gọi malloc/free, không exception, không RTTI, không STL allocations trong hot path), binary size mục tiêu ~20KB trên Windows, hành vi deterministic trên cả ba nền tảng.

## Glossary

- **Engine_Core**: Module `src/core/` gồm CayData, CayEngine, CayTypes — phần xử lý Telex thuần, không phụ thuộc OS.
- **Telex_Engine**: Class `Cay::TelexEngine` trong `CayEngine.h` — state machine xử lý từng phím và sinh ra output Unicode.
- **CayData**: Class helper static `Cay::CayData` chứa bảng nguyên âm, phụ âm, dấu thanh và các hàm map ký tự.
- **Tone_Index**: Số nguyên 0..5 chỉ dấu thanh (0=ngang, 1=huyền, 2=sắc, 3=hỏi, 4=ngã, 5=nặng) theo `GetToneIndex` trong CayData.
- **Tone_Key**: Phím Telex modifier dấu thanh: s, f, r, x, j, z (và viết hoa).
- **Hook_Key**: Phím Telex modifier `w` cho dấu mũ/móc/ngắn (ă, ơ, ư).
- **Double_Key**: Phím Telex lặp đôi (aa→â, ee→ê, oo→ô, dd→đ).
- **Backward_Scan**: Thuật toán scan ngược từ cuối `_text[]` về đầu để áp dụng modifier (QUY TẮC 3).
- **Bypass_Word**: Trạng thái engine quyết định buffer hiện tại là từ tiếng Anh và không áp dụng transform Telex (thông qua `ShouldBypassWord`).
- **Round_Trip_Property**: Tính chất `apply_then_undo(input) == input` — gõ một phím modifier rồi gõ lại chính phím đó phải khôi phục output về trạng thái trước khi áp.
- **Idempotency_Property**: Tính chất `apply(apply(x)) == apply(x)` — áp dụng cùng một thao tác hai lần cho ra cùng kết quả với một lần (áp dụng cho strip/reset).
- **Determinism_Property**: Cùng một chuỗi `KeyEvent` đầu vào, từ cùng một trạng thái khởi tạo, sinh ra cùng một chuỗi `OnInjectText` callbacks.
- **Tone_Position_Rule**: Quy tắc đặt dấu tiếng Việt (1 nguyên âm: tại nguyên âm; 2 nguyên âm có phụ âm cuối: nguyên âm cuối; 2 nguyên âm mở: nguyên âm đầu trừ ngoại lệ oa/oe/uê/uy/uơ/iê; 3 nguyên âm: nguyên âm giữa trừ uyê/giuô/giươ).
- **No_CRT_Constraint**: Trên Windows, code KHÔNG được gọi bất kỳ hàm CRT nào không có trong `no_crt.cpp`, KHÔNG dùng STL container, KHÔNG cấp phát động, KHÔNG throw exception.
- **Hot_Path**: Đường thực thi từ khi user nhấn phím đến khi `OnInjectText` được gọi — chạy trên mọi keystroke.
- **Single_Source_Of_Truth**: Mỗi loại bảng (initials, nuclei, finals, tone marks) chỉ tồn tại ở MỘT nơi duy nhất trong codebase.
- **Behavioral_Equivalence**: Với cùng một chuỗi input phím, output Unicode đầu ra của engine sau refactor phải đồng nhất với engine trước refactor.
- **MAX_BUFFER**: Hằng số kích thước buffer tối đa cho `_buffer`, `_text`, `_lastOutput`, `_savedBuffer`, `_savedText` trong `TelexEngine` (hiện là 64).
- **InjectText_Callback**: Hàm callback `InjectTextFunc` (định nghĩa trong CayTypes.h) mà platform layer cài đặt để engine yêu cầu xoá ký tự + chèn text mới.

## Requirements

### Requirement 1: Tính tương đương hành vi sau refactor (Behavioral Equivalence)

**User Story:** Là người dùng cuối Cay, tôi muốn engine sau refactor sinh ra cùng một output Telex như engine trước refactor, để việc gõ tiếng Việt của tôi không bị thay đổi hay gãy bất ngờ.

#### Acceptance Criteria

1. WHEN một chuỗi `KeyEvent` được đưa vào `TelexEngine::OnKeyDown` từ trạng thái khởi tạo, THE Engine_Core SHALL sinh ra cùng một chuỗi `(backspaceCount, newText, newTextLen)` trong `OnInjectText` như phiên bản trước refactor.
2. THE Engine_Core SHALL bảo toàn output Telex cho mọi từ trong corpus test cố định gồm tối thiểu 200 từ tiếng Việt phổ thông và 50 từ tiếng Anh, tại commit baseline trước refactor.
3. WHEN cùng một chuỗi phím được gõ trên Windows, macOS và Linux/Fcitx5, THE Engine_Core SHALL sinh ra cùng một chuỗi Unicode output (Determinism_Property).
4. WHEN refactor hoàn tất, THE Engine_Core SHALL pass toàn bộ test suite hiện hành (nếu có) cộng với test suite Property-Based mới được mô tả ở các requirement bên dưới.

### Requirement 2: Single source of truth cho bảng âm tiết tiếng Việt

**User Story:** Là người maintain code, tôi muốn mỗi bảng (phụ âm đầu, nguyên âm, phụ âm cuối, dấu thanh) chỉ tồn tại ở một nơi duy nhất, để khi cần sửa quy tắc tiếng Việt tôi không phải sửa nhiều chỗ và rủi ro các bảng bị lệch nhau.

#### Acceptance Criteria

1. THE Engine_Core SHALL định nghĩa bảng phụ âm đầu (`initials`), bảng nguyên âm (`nuclei`), bảng phụ âm cuối (`finals`) và bảng vần phụ (`tails`) tại đúng MỘT vị trí duy nhất trong `CayData.cpp`.
2. WHEN `IsCompleteSyllable` cần truy cập danh sách phụ âm đầu, nguyên âm, phụ âm cuối hoặc vần phụ, THE Telex_Engine SHALL gọi API public của CayData thay vì khai báo lại các bảng cục bộ trong `CayEngine.cpp`.
3. THE CayData SHALL expose API đủ để `IsCompleteSyllable` thực hiện được việc match phụ âm đầu/nguyên âm/phụ âm cuối/vần phụ mà KHÔNG cần Telex_Engine biết đến chi tiết bảng.
4. IF một entry cần được thêm hoặc sửa trong bảng phụ âm đầu, nguyên âm, phụ âm cuối hoặc vần phụ, THEN THE Engine_Core SHALL chỉ yêu cầu sửa tại đúng MỘT file (`CayData.cpp`).
5. THE Engine_Core SHALL KHÔNG còn duplicate giữa các entry `dd` và `\u0111` (đ) trong bảng phụ âm đầu. Bảng SHALL chỉ chứa cụm phụ âm đầu Telex chuẩn (b, c, ch, d, đ, g, gh, gi, h, k, kh, l, m, n, ng, ngh, nh, p, ph, qu, r, s, t, th, tr, v, x), KHÔNG chứa biến thể trùng lặp.

### Requirement 3: Làm sạch bảng nguyên âm (s_nuclei)

**User Story:** Là người maintain code, tôi muốn bảng nguyên âm chỉ chứa nguyên âm tiếng Việt hợp lệ và không có rác/placeholder, để bảng phản ánh đúng ngữ pháp tiếng Việt và an toàn khi được tham chiếu trong tương lai.

#### Acceptance Criteria

1. THE bảng nguyên âm trong CayData SHALL KHÔNG chứa entry `ăn` (đây là vần có phụ âm cuối, không phải nucleus).
2. THE bảng nguyên âm trong CayData SHALL KHÔNG chứa các tổ hợp không hợp lệ trong tiếng Việt: `yi`, `yo`, `yu`, `ou`.
3. THE bảng nguyên âm trong CayData SHALL KHÔNG chứa entry trùng lặp; cụ thể `âu` SHALL chỉ xuất hiện một lần.
4. WHEN bảng nguyên âm được sửa, THE Engine_Core SHALL bảo toàn Behavioral_Equivalence (Requirement 1) — nghĩa là việc xoá entry rác KHÔNG được làm thay đổi output cho từ tiếng Việt hợp lệ nào.
5. IF `IsValidNucleus` được gọi với một chuỗi nguyên âm tiếng Việt hợp lệ bất kỳ trong corpus test, THEN THE CayData SHALL trả về `true`.
6. IF `IsValidNucleus` được gọi với một chuỗi không phải nguyên âm tiếng Việt (ví dụ `yi`, `yo`, `ou`, `ăn`), THEN THE CayData SHALL trả về `false`.

### Requirement 4: Loại bỏ code lặp khi xử lý nguyên âm có dấu

**User Story:** Là developer, tôi muốn logic "tách base + tone + isUpper" và "ghép base mới + tone cũ + uppercase" được đóng gói thành helper, để code engine ngắn hơn, dễ đọc, dễ kiểm tra và ít rủi ro lỗi khi sửa.

#### Acceptance Criteria

1. THE Engine_Core SHALL cung cấp helper function (trong CayData hoặc namespace `Cay`) để tách một ký tự tiếng Việt thành bộ ba `(base, toneIndex, isUpper)`.
2. THE Engine_Core SHALL cung cấp helper function tương ứng để ghép `(newBase, toneIndex, isUpper)` trở lại ký tự tiếng Việt có dấu (hoặc ký tự cơ bản nếu `toneIndex == 0`).
3. WHEN helper tách `(base, toneIndex, isUpper)` được áp dụng cho ký tự `c`, THE Engine_Core SHALL thoả tính chất round-trip: ghép lại từ bộ ba đó SHALL cho ra đúng `c` (Round_Trip_Property cho mọi ký tự `c` thuộc tập nguyên âm tiếng Việt thuần và có dấu được CayData hỗ trợ).
4. THE function `ApplyDoubleKeys` trong `CayEngine.cpp` SHALL được rewrite để dùng các helper trên, và SHALL KHÔNG còn lặp lại block code "extract tone" (vòng for `for t = 1..5`) quá MỘT lần trong toàn bộ thân hàm.
5. THE function `ApplyHookKeys` trong `CayEngine.cpp` SHALL được rewrite để dùng các helper trên, và SHALL KHÔNG còn lặp lại block code "extract tone" quá MỘT lần trong toàn bộ thân hàm.
6. THE function `ApplyToneMarks` trong `CayEngine.cpp` SHALL dùng cùng helper "tách/ghép" thay cho block code lặp.
7. WHEN refactor hoàn tất, THE tổng số dòng `for (int t = 1; t <= 5; t++)` trùng pattern "extract current tone" trong `CayEngine.cpp` SHALL được giảm xuống tối đa 1 lần (hiện tại >10 lần).

### Requirement 5: Hằng số kích thước buffer thống nhất

**User Story:** Là developer, tôi muốn mọi hằng số kích thước buffer trong engine và platform layer có nguồn duy nhất hoặc được đặt tên rõ ràng, để tránh silent truncation và để người đọc hiểu ý nghĩa của từng số.

#### Acceptance Criteria

1. THE Engine_Core SHALL định nghĩa `MAX_BUFFER` tại đúng MỘT vị trí duy nhất (hiện ở `CayData.h`) và SHALL được include từ `CayTypes.h` hoặc tương đương để sử dụng nhất quán.
2. WHEN Telex_Engine khai báo các array nội bộ (`_buffer`, `_text`, `_lastOutput`, `_savedBuffer`, `_savedText`), THE Engine_Core SHALL sử dụng đúng cùng một hằng số `MAX_BUFFER`.
3. THE platform layer (Windows InputInjector, MacInputInjector, Fcitx5 CayimeEngine) SHALL sử dụng cùng `MAX_BUFFER` (hoặc hằng số có tên rõ ràng phái sinh từ `MAX_BUFFER`) cho buffer trung gian khi xử lý `newText` từ `InjectText_Callback`, hoặc SHALL document rõ lý do cần kích thước khác.
4. IF `newTextLen` truyền vào `InjectText_Callback` lớn hơn kích thước buffer trung gian của platform layer, THEN THE platform layer SHALL KHÔNG silent truncation; nó SHALL hoặc xử lý đầy đủ (cấp phát đủ chỗ) hoặc trả về lỗi/log rõ ràng.
5. THE MacInputInjector SHALL KHÔNG còn array cố định `chars[64]` mà không liên hệ với `MAX_BUFFER`; nó SHALL dùng `MAX_BUFFER` hoặc kích thước ≥ `MAX_BUFFER`.

### Requirement 6: Tính idempotency của các phím modifier

**User Story:** Là người dùng Telex, tôi muốn gõ cùng một phím dấu thanh hai lần phải undo dấu (cho ra ký tự không dấu + ký tự dấu raw), để có thể sửa lỗi gõ nhầm dấu một cách trực quan.

#### Acceptance Criteria

1. WHEN user gõ chuỗi `[base_keys][tone_key][tone_key]` từ trạng thái rỗng và `[base_keys]` đại diện cho một âm tiết Telex hợp lệ, THE Telex_Engine SHALL sinh output cuối cùng tương đương với output của chuỗi `[base_keys][tone_key_raw]` (dấu thanh đã được undo, ký tự `tone_key` được append nguyên dạng).
2. WHEN user gõ chuỗi `aa` từ trạng thái rỗng tiếp theo là `a` (chuỗi `aaa`), THE Telex_Engine SHALL sinh output `aa` (Round_Trip_Property của Double_Key).
3. WHEN user gõ chuỗi tạo `ơ` (ví dụ `ow`) tiếp theo là `w`, THE Telex_Engine SHALL undo hook và sinh output `ow` (Round_Trip_Property của Hook_Key).
4. WHEN user gõ phím `z` sau khi từ hiện tại đã có dấu thanh, THE Telex_Engine SHALL xoá dấu thanh hiện tại VÀ KHÔNG append ký tự `z` vào output.
5. WHEN user gõ phím `z` sau khi từ hiện tại KHÔNG có dấu thanh, THE Telex_Engine SHALL append ký tự `z` vào output bình thường.
6. THE refactor SHALL bảo toàn các tính chất Round_Trip ở 4 acceptance criteria trên cho cả tổ hợp với chữ hoa và chữ thường.

### Requirement 7: Đặt dấu thanh đúng vị trí theo quy tắc tiếng Việt

**User Story:** Là người dùng Telex, tôi muốn dấu thanh được đặt đúng vị trí nguyên âm theo quy tắc tiếng Việt chuẩn cho mọi loại âm tiết, để output đúng ngữ pháp.

#### Acceptance Criteria

1. WHEN âm tiết có 1 nguyên âm trong `_text`, THE Telex_Engine SHALL đặt dấu thanh tại nguyên âm duy nhất đó.
2. WHEN âm tiết có 2 nguyên âm và CÓ phụ âm cuối, THE Telex_Engine SHALL đặt dấu thanh tại nguyên âm thứ hai (ví dụ `tuấn`, `điện`).
3. WHEN âm tiết có 2 nguyên âm KHÔNG có phụ âm cuối và bộ đôi nguyên âm là một trong `oa, oe, uê, uy, uơ, iê` (xét theo quy tắc trong `FindTonePosition`), THE Telex_Engine SHALL đặt dấu thanh tại nguyên âm thứ hai.
4. WHEN âm tiết có 2 nguyên âm KHÔNG có phụ âm cuối và bộ đôi nguyên âm KHÔNG thuộc danh sách trên, THE Telex_Engine SHALL đặt dấu thanh tại nguyên âm thứ nhất (ví dụ `rồi, mèo, đôi, bơi, múa`).
5. WHEN âm tiết có 3 nguyên âm và CÓ phụ âm cuối, THE Telex_Engine SHALL đặt dấu thanh tại nguyên âm cuối (ví dụ `tuyến, giường`).
6. WHEN âm tiết có 3 nguyên âm KHÔNG có phụ âm cuối, THE Telex_Engine SHALL đặt dấu thanh tại nguyên âm giữa trừ các trường hợp đặc biệt `uyê, giuô, giươ` (đặt tại nguyên âm cuối).
7. WHEN nguyên âm `qu` + nguyên âm hoặc `gi` + nguyên âm (2 nguyên âm bắt đầu bởi `u/i` sau `q/g`), THE Telex_Engine SHALL đặt dấu thanh tại nguyên âm đứng sau (`q`/`g` được coi là phụ âm).
8. THE refactor SHALL bảo toàn 100% logic của `FindTonePosition` theo các criteria trên (Behavioral_Equivalence).

### Requirement 8: Bypass đúng cho từ tiếng Anh

**User Story:** Là lập trình viên dùng Cay, tôi muốn các từ tiếng Anh / camelCase / snake_case không bị áp dụng dấu Telex, để code và identifier không bị "bẻ" thành tiếng Việt.

#### Acceptance Criteria

1. WHEN buffer hiện tại bắt đầu bằng một trong các ký tự `w, f, j, z` (chữ thường, phím đầu của từ), THE Telex_Engine SHALL trả về `true` từ `ShouldBypassWord`.
2. WHEN buffer hiện tại bắt đầu bằng `q` mà ký tự thứ hai KHÔNG phải `u`, THE Telex_Engine SHALL trả về `true` từ `ShouldBypassWord`.
3. WHEN buffer hiện tại bắt đầu bằng `p` mà ký tự thứ hai KHÔNG phải `h`, THE Telex_Engine SHALL trả về `true` từ `ShouldBypassWord`.
4. WHEN buffer hiện tại có cụm phụ âm kép đầu từ KHÔNG thuộc 8 cụm tiếng Việt hợp lệ (`ch, gh, kh, ng, nh, ph, th, tr, dd`), THE Telex_Engine SHALL trả về `true` từ `ShouldBypassWord` (ví dụ `class, style, block`).
5. WHEN âm tiết hiện tại đã có dấu thanh trong các nguyên âm `huyền, hỏi, ngã` đứng trước phụ âm cuối thuộc `c, ch, p, t`, THE Telex_Engine SHALL trả về `true` từ `ShouldBypassWord` (Strict Tone-Final Consonant Rule).
6. WHEN âm tiết KHÔNG vượt qua được `IsCompleteSyllable` (cấu trúc không hợp lệ), THE Telex_Engine SHALL trả về `true` từ `ShouldBypassWord`.
7. THE refactor SHALL bảo toàn toàn bộ các quy tắc bypass trên (Behavioral_Equivalence cho `ShouldBypassWord`).

### Requirement 9: Round-trip cho serialization của ký tự có dấu

**User Story:** Là maintainer, tôi muốn các hàm `StripTone` / `StripAccent` / `GetToneMark` thoả mãn tính chất round-trip rõ ràng, để biết chắc rằng các bảng dấu trong CayData là nhất quán và đầy đủ.

#### Acceptance Criteria

1. FOR ALL nguyên âm cơ bản `b` (a, â, ă, e, ê, i, o, ô, ơ, u, ư, y) và mọi `toneIndex` từ 0 đến 5, IF `c = GetToneMark(b, toneIndex)` và `c != 0`, THEN `StripTone(c) == b` (Round_Trip_Property của tone).
2. FOR ALL ký tự nguyên âm có dấu `c` mà `HasVietnameseMark(c) == true`, THE CayData SHALL thoả mãn `StripAccent(StripTone(c))` là một nguyên âm ASCII thuần `(a, e, i, o, u, y)` hoặc tương đương chữ hoa.
3. FOR ALL nguyên âm cơ bản hợp lệ `b` và mọi `toneIndex` 1..5, THE CayData SHALL trả về một codepoint khác 0 từ `GetToneMark(b, toneIndex)`.
4. FOR ALL `toneIndex` 0..5 và phím Telex modifier dấu thanh tương ứng `k` (z, f, s, r, x, j), THE CayData SHALL thoả `GetToneIndex(k) == toneIndex` (cả chữ thường và chữ hoa).
5. WHEN có thay đổi trong bảng dấu của CayData, THE Engine_Core SHALL pass test round-trip cho toàn bộ tập nguyên âm tiếng Việt được hỗ trợ.

### Requirement 10: Idempotency của StripAllTones và ResetState

**User Story:** Là maintainer, tôi muốn các thao tác "reset/strip" có tính idempotent rõ ràng, để engine không có hidden state lệch sau hai lần gọi liên tiếp.

#### Acceptance Criteria

1. WHEN `StripAllTones` được gọi hai lần liên tiếp, THE Telex_Engine SHALL có `_text[]` không thay đổi giữa lần thứ nhất và lần thứ hai (Idempotency_Property).
2. WHEN `ResetState` được gọi hai lần liên tiếp, THE Telex_Engine SHALL có toàn bộ trường nội bộ (`_bufferCount`, `_textLen`, `_toneIndex`, `_lastOutputLen`) bằng nhau giữa lần thứ nhất và lần thứ hai.
3. WHEN `ResetFull` được gọi sau bất kỳ chuỗi `OnKeyDown` nào, THE Telex_Engine SHALL trở về cùng một trạng thái với việc khởi tạo `TelexEngine()` mới (so với mọi field công khai và nội bộ qua API debug nếu có).

### Requirement 11: Ràng buộc no-CRT, no-allocation, no-exception

**User Story:** Là người chịu trách nhiệm binary size trên Windows, tôi muốn refactor không phá vỡ ràng buộc no-CRT của bản Windows, để binary cuối cùng vẫn chạy được không cần CRT runtime.

#### Acceptance Criteria

1. THE Engine_Core SHALL KHÔNG thêm bất kỳ include nào của header STL (`<vector>`, `<string>`, `<map>`, `<unordered_map>`, `<algorithm>`, ...) vào `src/core/`.
2. THE Engine_Core SHALL KHÔNG thực hiện cấp phát động (`new`, `malloc`) trong Hot_Path.
3. THE Engine_Core SHALL KHÔNG `throw` exception và SHALL KHÔNG dùng cấu trúc `try/catch`.
4. THE Engine_Core SHALL KHÔNG thêm dependency mới đòi hỏi RTTI.
5. WHEN build cấu hình Windows Release, THE binary SHALL link thành công với cờ `/EHs-c-`, `/GR-`, `/NODEFAULTLIB:msvcrt.lib` như trong `CMakeLists.txt`.
6. THE Engine_Core SHALL chỉ dùng các helper string đã có (`CayStrLen`, `CayStrCmp`) hoặc helper mới được thêm vào `CayTypes.h` mà KHÔNG gọi vào CRT cho việc xử lý chuỗi.

### Requirement 12: Mục tiêu binary size

**User Story:** Là maintainer dự án, tôi muốn refactor không làm tăng binary size mục tiêu (~20KB Windows, ~40KB macOS, ~30KB Linux), để giữ định vị "siêu nhẹ" của Cay.

#### Acceptance Criteria

1. WHEN build Windows Release với cùng compiler và cờ trong `CMakeLists.txt`, THE binary `cay.exe` SHALL có kích thước ≤ 110% so với baseline trước refactor.
2. WHEN build macOS Release, THE bundle `cay.app` (binary chính) SHALL có kích thước ≤ 110% so với baseline trước refactor.
3. WHEN build Fcitx5 plugin Release, THE shared object SHALL có kích thước ≤ 110% so với baseline trước refactor.
4. THE refactor PROCESS SHALL ghi nhận kích thước trước-sau ở phase implementation và đưa vào tài liệu task hoàn thành (mục tiêu lý tưởng: ≤ 100%).

### Requirement 13: Performance Hot_Path

**User Story:** Là người dùng cuối, tôi muốn Cay không bị giật khi gõ nhanh, để trải nghiệm gõ tự nhiên.

#### Acceptance Criteria

1. WHEN xử lý một `KeyEvent` đơn lẻ qua `OnKeyDown`, THE Telex_Engine SHALL thực hiện `O(MAX_BUFFER)` thao tác trên `_buffer` và `_text`.
2. THE Engine_Core SHALL KHÔNG có cấp phát heap nào trong Hot_Path (verify bằng review code và bằng test debug).
3. THE Engine_Core SHALL KHÔNG có vòng lặp lồng có độ phức tạp lớn hơn `O(MAX_BUFFER^2)` trong `OnKeyDown`.
4. WHEN một `KeyEvent` gây ra refactor liên quan tới scan ngược, THE Engine_Core SHALL chỉ thực hiện đúng MỘT pass scan ngược để xác định vị trí áp modifier (bỏ logic "for t=1..5 extract tone" trùng lặp ở mỗi iteration nhờ helper từ Requirement 4).

### Requirement 14: Loại bỏ dead code và placeholder

**User Story:** Là maintainer, tôi muốn engine không còn dead code và placeholder lạc lõng, để codebase phản ánh đúng những gì đang thực sự chạy.

#### Acceptance Criteria

1. IF một hàm public trong CayData không được gọi từ bất kỳ đâu trong codebase (verify bằng grep toàn workspace), THEN THE refactor SHALL hoặc xoá hàm đó hoặc cung cấp lý do rõ ràng vì sao giữ lại (ví dụ public API dự kiến) trong comment ngay phía trên.
2. THE function `OnKeyUp` SHALL hoặc bị xoá khỏi public API (cùng với cập nhật platform layer nếu cần) hoặc được giữ lại với comment rõ ràng giải thích lý do (ví dụ: contract của platform layer cần endpoint).
3. THE bảng `s_nuclei` trong `CayData.cpp` SHALL được rà soát cùng quy tắc Requirement 3 và mọi entry placeholder ("để làm tròn đến 48", `ăn`, ...) SHALL bị xoá.
4. IF `IsValidNucleus` không được gọi từ đâu sau khi refactor (Requirement 2 yêu cầu Telex_Engine dùng API CayData), THEN THE refactor SHALL hoặc xoá hàm `IsValidNucleus` hoặc đảm bảo nó là một phần của API public mới mà `IsCompleteSyllable` thực sự sử dụng.

### Requirement 15: Sửa mojibake trong comments

**User Story:** Là người đọc code, tôi muốn comment trong source được encode UTF-8 đúng tiếng Việt, để hiểu được ý đồ ban đầu mà không cần đoán mò.

#### Acceptance Criteria

1. THE files trong `src/core/` SHALL được lưu dưới dạng UTF-8 không có BOM.
2. THE refactor SHALL sửa toàn bộ comment bị mojibake (chuỗi như "Lu?t Q", "B?t bu?c", "Ti?ng Vi?t", "?", "T?I ��Y") trong `CayEngine.cpp` thành tiếng Việt UTF-8 đúng nghĩa.
3. WHEN refactor hoàn tất, THE `git grep` cho pattern `\\?` trong `src/core/*.cpp` SHALL chỉ tìm thấy các dấu `?` thuộc về cú pháp ngôn ngữ (ternary, regex), không phải comment tiếng Việt vỡ.

### Requirement 16: Hardcoded terminal list trên Fcitx5 (lowest priority)

**User Story:** Là người dùng Linux, tôi muốn engine không lệ thuộc vào danh sách hardcoded các terminal emulator để xử lý fallback Backspace, để khi terminal mới ra đời tôi không cần đợi update.

#### Acceptance Criteria

1. THE Fcitx5 platform layer SHALL document rõ trong comment lý do vì sao cần fallback `forwardKey(BackSpace)` thay cho `deleteSurroundingText` đối với terminal emulators (do bug VTE / Surrounding Text không reliable).
2. THE Fcitx5 platform layer SHALL cung cấp cơ chế cho phép mở rộng danh sách terminal program names mà KHÔNG phải sửa source mã (ví dụ: cấu hình bên trong `cayime-im.conf` hoặc tương đương) HOẶC SHALL document rõ trong README_FCITX5.md cách build lại với danh sách mới — chọn một, đảm bảo không silent.
3. IF việc mở rộng cấu hình runtime tạo gánh nặng kích thước hoặc phức tạp, THEN THE refactor MAY giữ danh sách hardcoded NHƯNG SHALL chuyển nó vào một const array có tên rõ ràng (`g_terminalProgramNames`) ở đầu file để dễ sửa và dễ tìm.
4. THE refactor SHALL KHÔNG mở rộng phạm vi sang xử lý hành vi terminal khác (chỉ làm sạch danh sách hiện có).

### Requirement 17: Parser/printer round-trip cho IsCompleteSyllable

**User Story:** Là maintainer, tôi muốn `IsCompleteSyllable` được kiểm tra với tính chất round-trip qua một pretty-printer của âm tiết, để biết chắc rằng phép parse là consistent.

#### Acceptance Criteria

1. THE Engine_Core SHALL cung cấp một hàm pretty-printer (có thể là helper test) sinh ra một chuỗi âm tiết tiếng Việt hợp lệ ở dạng ASCII Telex (ví dụ tổ hợp `initial + nucleus + final + tail`) từ một bộ index `(initialIdx, nucleusIdx, finalIdx, tailIdx)`.
2. FOR ALL bộ index `(initialIdx, nucleusIdx, finalIdx, tailIdx)` mà pretty-printer sinh ra một chuỗi `s` không vi phạm `Nucleus-Final Pairing Rule` đã quy định, THE function `IsCompleteSyllable(s, len(s))` SHALL trả về `true` (Round_Trip_Property: print → parse).
3. FOR ALL chuỗi `s` không thoả `IsCompleteSyllable`, THE Engine_Core SHALL KHÔNG cung cấp bất kỳ bộ index nào mà pretty-printer sinh ra `s` (Round_Trip_Property: ngược).
4. WHEN refactor hoàn tất, THE round-trip property này SHALL được kiểm tra bằng property-based test với ≥ 200 cases đa dạng nucleus và final.

### Requirement 18: Bảo toàn `_canRestore` và Restore on Backspace

**User Story:** Là người dùng cuối, tôi muốn sau khi commit một từ bằng Space, tôi vẫn có thể nhấn Backspace để khôi phục từ vừa commit (`_canRestore = true`), để sửa nhanh từ vừa gõ.

#### Acceptance Criteria

1. WHEN một từ vừa được commit qua Space (và buffer trước đó không rỗng), THE Telex_Engine SHALL set `_canRestore = true` và lưu `_savedBuffer`, `_savedText`, `_savedToneIndex`.
2. WHEN người dùng nhấn Backspace ngay sau commit (buffer hiện tại rỗng VÀ `_canRestore == true`), THE Telex_Engine SHALL restore `_buffer`, `_text`, `_toneIndex`, `_lastOutput` từ saved state và set `_canRestore = false`.
3. THE refactor SHALL KHÔNG thay đổi behavior `_canRestore` được mô tả trong code hiện tại (Behavioral_Equivalence).

### Requirement 19: Tài liệu hoá quyết định kiến trúc của refactor

**User Story:** Là maintainer tương lai, tôi muốn hiểu được quyết định thiết kế của refactor (helper API, single source of truth, no-CRT), để tránh quay ngược các thay đổi vô tình.

#### Acceptance Criteria

1. WHEN refactor hoàn tất, THE design document trong `.kiro/specs/engine-core-refactor/` SHALL ghi rõ: (a) API helper mới được expose từ CayData, (b) thay đổi của public API (nếu có) trên TelexEngine, (c) ràng buộc no-CRT đã được verify như thế nào.
2. THE refactor SHALL bao gồm comment header trong các file `src/core/*.h` chỉ ra single source of truth của các bảng và quy tắc no-CRT.
3. IF có thay đổi public API (ví dụ thêm helper `DecomposeChar` / `ComposeChar` vào CayData), THEN THE refactor SHALL cập nhật `README.md` hoặc tài liệu tương đương khi và chỉ khi public API đó hữu ích cho người dùng ngoài Engine_Core.
