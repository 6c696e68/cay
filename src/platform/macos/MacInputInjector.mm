#import "MacInputInjector.h"
#import "CayTypes.h"
#import <CoreGraphics/CoreGraphics.h>
#import <unistd.h>

namespace CayIME {

// Cờ đánh dấu các event do bộ gõ tự tạo, tránh loop
const int CAY_EVENT_MAGIC_FLAG = 9999;

// Cờ báo hiệu vừa có sự kiện thay thế văn bản xảy ra
bool g_justInjected = false;

// Timing (microseconds)
static const useconds_t kSelectDelay  = 1000;  // 1ms giữa mỗi Shift+Left
static const useconds_t kWaitAfterSel = 3000;  // 3ms chờ sau selection, trước text
static const useconds_t kSettleTime   = 3000;  // 3ms settle sau inject text

static CGEventSourceRef GetPrivateEventSource() {
    static CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStatePrivate);
    return src;
}

static void PostKeyEvent(CGKeyCode keyCode, bool isKeyDown, CGEventFlags flags = 0) {
    CGEventSourceRef src = GetPrivateEventSource();
    CGEventRef event = CGEventCreateKeyboardEvent(src, keyCode, isKeyDown);
    if (event) {
        if (flags) CGEventSetFlags(event, flags);
        CGEventSetIntegerValueField(event, kCGEventSourceUserData, CAY_EVENT_MAGIC_FLAG);
        CGEventPost(kCGSessionEventTap, event);
        CFRelease(event);
    }
}

static void PostUnicodeString(const UniChar* chars, size_t length) {
    CGEventSourceRef src = GetPrivateEventSource();
    CGEventRef event = CGEventCreateKeyboardEvent(src, 0, true);
    if (event) {
        CGEventKeyboardSetUnicodeString(event, length, chars);
        CGEventSetIntegerValueField(event, kCGEventSourceUserData, CAY_EVENT_MAGIC_FLAG);
        CGEventPost(kCGSessionEventTap, event);
        CFRelease(event);
    }
}

void MacInputInjector::ReplaceText(int backspaceCount, const wchar_t* newText, int newTextLen) {
    g_justInjected = true;

    // Selection method: Shift+Left × N để select N ký tự, sau đó gõ text mới thay thế.
    // Đáng tin cậy hơn backspace vì:
    //  - Không bị Spotlight autocomplete nuốt backspace
    //  - Text mới thay thế selection atomic (không race condition)
    //  - Hoạt động đúng trên mọi app (Finder, TextEdit, Spotlight, Chrome...)
    if (backspaceCount > 0) {
        // kVK_LeftArrow = 123
        for (int i = 0; i < backspaceCount; ++i) {
            PostKeyEvent(123, true, kCGEventFlagMaskShift);
            PostKeyEvent(123, false, kCGEventFlagMaskShift);
            usleep(kSelectDelay);
        }
        usleep(kWaitAfterSel);
    }

    // Gửi Unicode string mới (thay thế selection, hoặc insert nếu không có selection)
    if (newText != nullptr && newTextLen > 0) {
        UniChar chars[Cay::MAX_BUFFER];
        int uniLen = 0;
        for (int i = 0; i < newTextLen && uniLen < Cay::MAX_BUFFER; ++i) {
            chars[uniLen++] = (UniChar)newText[i];
        }
        PostUnicodeString(chars, uniLen);
    } else if (backspaceCount > 0) {
        // Chỉ xóa (không có text mới): gửi backspace/delete để xóa selection
        PostKeyEvent(51, true);   // kVK_Delete
        PostKeyEvent(51, false);
    }

    usleep(kSettleTime);
}

} // namespace CayIME
