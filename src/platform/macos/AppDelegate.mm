#import "AppDelegate.h"
#import "MacHookManager.h"
#import "MacInputInjector.h"
#import "CayEngine.h"
#import <ServiceManagement/ServiceManagement.h>
#import <ApplicationServices/ApplicationServices.h>

namespace CayIME {
    Cay::TelexEngine g_engine;
    extern bool g_enabled;
}

@implementation AppDelegate {
    NSStatusItem *statusItem;
    NSTimer *accessibilityPollTimer;
    BOOL hookInitialized;
    BOOL askedFirstRunAutoStart;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // 1. Single Instance Check
    NSArray *apps = [NSRunningApplication runningApplicationsWithBundleIdentifier:[[NSBundle mainBundle] bundleIdentifier]];
    if ([apps count] > 1) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Cay đang chạy!"];
        [alert runModal];
        [NSApp terminate:nil];
        return;
    }

    // 2. Gắn callback Inject Text trước
    CayIME::g_engine.OnInjectText = CayIME::MacInputInjector::ReplaceText;

    // 3. Setup status menu LUÔN, kể cả khi chưa có quyền — để user còn thấy app sống
    [self setupMenu];

    // 4. Lắng nghe notification toggle (từ HookManager khi nhấn Cmd+Shift)
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(handleToggleNotification:)
                                                 name:@"ToggleIMENotification"
                                               object:nil];

    // 5. Cố gắng khởi tạo hook
    [self tryInitializeHook];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    if (accessibilityPollTimer) {
        [accessibilityPollTimer invalidate];
        accessibilityPollTimer = nil;
    }
    CayIME::MacHookManager::Shutdown();
}

#pragma mark - Hook lifecycle

- (void)tryInitializeHook {
    if (hookInitialized) return;

    if (!CayIME::MacHookManager::IsAccessibilityTrusted()) {
        // Hỏi macOS bật prompt cấp quyền (hệ thống chỉ hiện 1 lần đầu)
        CayIME::MacHookManager::RequestAccessibilityPrompt();
        [self showAccessibilityNeededAlertOnce];
        [self startAccessibilityPolling];
        [self updateIcon];
        return;
    }

    if (CayIME::MacHookManager::Initialize()) {
        hookInitialized = YES;
        if (accessibilityPollTimer) {
            [accessibilityPollTimer invalidate];
            accessibilityPollTimer = nil;
        }
        [self updateIcon];
        [self maybeAskFirstRunAutoStart];
    } else {
        // Có quyền mà tap vẫn fail (hiếm) — báo lỗi cứng
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Không thể khởi tạo Hook bàn phím."];
        [alert setInformativeText:@"Cay đã có quyền Accessibility nhưng không tạo được Event Tap. Vui lòng khởi động lại app."];
        [alert addButtonWithTitle:@"Đóng"];
        [alert runModal];
    }
}

- (void)showAccessibilityNeededAlertOnce {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Cay cần quyền Accessibility"];
    [alert setInformativeText:
        @"Để bắt phím Telex, hãy bật Cay trong:\n"
        @"System Settings → Privacy & Security → Accessibility.\n\n"
        @"Nếu bạn vừa cài lại Cay và checkbox vẫn không nhận quyền, hãy bấm "
        @"\"Reset quyền cũ\" để xoá entry cũ trong macOS rồi bật lại từ đầu.\n\n"
        @"Sau khi bật, Cay sẽ tự nhận quyền — không cần thoát app."];
    [alert addButtonWithTitle:@"Mở System Settings"];
    [alert addButtonWithTitle:@"Reset quyền cũ"];
    [alert addButtonWithTitle:@"Để sau"];
    [NSApp activateIgnoringOtherApps:YES];
    NSModalResponse resp = [alert runModal];
    if (resp == NSAlertFirstButtonReturn) {
        [self openAccessibilitySettings];
    } else if (resp == NSAlertSecondButtonReturn) {
        [self resetAccessibilityTCC];
    }
}

- (void)resetAccessibilityTCC {
    NSString *bundleID = [[NSBundle mainBundle] bundleIdentifier];
    NSTask *task = [[NSTask alloc] init];
    task.launchPath = @"/usr/bin/tccutil";
    task.arguments = @[@"reset", @"Accessibility", bundleID];
    @try { [task launch]; [task waitUntilExit]; } @catch (__unused NSException *e) {}

    NSAlert *done = [[NSAlert alloc] init];
    [done setMessageText:@"Đã reset quyền Accessibility cho Cay"];
    [done setInformativeText:@"Bây giờ mở System Settings → Privacy & Security → Accessibility, "
                              @"thêm /Applications/cay.app và bật công tắc."];
    [done addButtonWithTitle:@"Mở System Settings"];
    [done addButtonWithTitle:@"Đóng"];
    if ([done runModal] == NSAlertFirstButtonReturn) {
        [self openAccessibilitySettings];
    }
}

- (void)startAccessibilityPolling {
    if (accessibilityPollTimer) return;
    accessibilityPollTimer = [NSTimer scheduledTimerWithTimeInterval:1.0
                                                              target:self
                                                            selector:@selector(pollAccessibility:)
                                                            userInfo:nil
                                                             repeats:YES];
}

- (void)pollAccessibility:(NSTimer *)timer {
    if (CayIME::MacHookManager::IsAccessibilityTrusted()) {
        [self tryInitializeHook];
    }
}

#pragma mark - Menu

- (void)setupMenu {
    statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    [self updateIcon];

    NSMenu *menu = [[NSMenu alloc] init];
    menu.delegate = self;

    NSMenuItem *toggleItem = [[NSMenuItem alloc] initWithTitle:@"Bật / Tắt (Cmd + Shift)"
                                                        action:@selector(toggleFromMenu)
                                                 keyEquivalent:@""];
    [menu addItem:toggleItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *grantItem = [[NSMenuItem alloc] initWithTitle:@"Cấp quyền Accessibility…"
                                                       action:@selector(openAccessibilitySettings)
                                                keyEquivalent:@""];
    grantItem.tag = 1001;
    [menu addItem:grantItem];

    NSMenuItem *resetItem = [[NSMenuItem alloc] initWithTitle:@"Reset quyền Accessibility"
                                                       action:@selector(resetAccessibilityTCC)
                                                keyEquivalent:@""];
    resetItem.tag = 1002;
    [menu addItem:resetItem];

    NSMenuItem *autoStartItem = [[NSMenuItem alloc] initWithTitle:@"Khởi động cùng macOS"
                                                           action:@selector(toggleAutoStart:)
                                                    keyEquivalent:@""];
    NSString *bundleID = [[NSBundle mainBundle] bundleIdentifier];
    NSString *plistPath = [NSString stringWithFormat:@"%@/Library/LaunchAgents/%@.plist",
                                                    NSHomeDirectory(), bundleID];
    if ([[NSFileManager defaultManager] fileExistsAtPath:plistPath]) {
        [autoStartItem setState:NSControlStateValueOn];
    } else {
        [autoStartItem setState:NSControlStateValueOff];
    }
    [menu addItem:autoStartItem];

    NSMenuItem *aboutItem = [[NSMenuItem alloc] initWithTitle:@"Giới thiệu"
                                                       action:@selector(showAbout)
                                                keyEquivalent:@""];
    [menu addItem:aboutItem];

    NSMenuItem *quitItem = [[NSMenuItem alloc] initWithTitle:@"Thoát"
                                                     action:@selector(quitApp)
                                              keyEquivalent:@"q"];
    [menu addItem:quitItem];

    statusItem.menu = menu;
}

- (void)menuNeedsUpdate:(NSMenu *)menu {
    BOOL trusted = CayIME::MacHookManager::IsAccessibilityTrusted();
    NSMenuItem *grantItem = [menu itemWithTag:1001];
    if (grantItem) grantItem.hidden = trusted;
    NSMenuItem *resetItem = [menu itemWithTag:1002];
    if (resetItem) resetItem.hidden = trusted;
}

- (void)updateIcon {
    NSString *text;
    if (!hookInitialized) {
        text = @"Cay: !"; // Cảnh báo chưa hoạt động
    } else {
        text = CayIME::g_enabled ? @"Cay: V" : @"Cay: E";
    }
    if (statusItem.button) {
        statusItem.button.title = text;
    }
}

- (void)openAccessibilitySettings {
    NSURL *url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"];
    [[NSWorkspace sharedWorkspace] openURL:url];
}

- (void)toggleFromMenu {
    [self toggleIME];
}

- (void)handleToggleNotification:(NSNotification *)notif {
    [self toggleIME];
}

- (void)toggleIME {
    if (!hookInitialized) {
        [self tryInitializeHook];
        return;
    }
    CayIME::g_enabled = !CayIME::g_enabled;
    CayIME::MacHookManager::ResetEngine();
    [self updateIcon];
}

- (void)toggleAutoStart:(NSMenuItem *)item {
    NSString *bundleID = [[NSBundle mainBundle] bundleIdentifier];
    NSString *execPath = [[NSBundle mainBundle] executablePath];
    NSString *launchAgentsDir = [NSString stringWithFormat:@"%@/Library/LaunchAgents", NSHomeDirectory()];
    NSString *plistPath = [NSString stringWithFormat:@"%@/%@.plist", launchAgentsDir, bundleID];

    NSFileManager *fm = [NSFileManager defaultManager];

    if ([fm fileExistsAtPath:plistPath]) {
        [fm removeItemAtPath:plistPath error:nil];
        [item setState:NSControlStateValueOff];
    } else {
        if (![fm fileExistsAtPath:launchAgentsDir]) {
            [fm createDirectoryAtPath:launchAgentsDir
          withIntermediateDirectories:YES
                           attributes:nil
                                error:nil];
        }

        NSDictionary *plistDict = @{
            @"Label": bundleID,
            @"ProgramArguments": @[execPath],
            @"RunAtLoad": @YES
        };

        [plistDict writeToFile:plistPath atomically:YES];
        [item setState:NSControlStateValueOn];
    }
}

- (void)maybeAskFirstRunAutoStart {
    if (askedFirstRunAutoStart) return;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults boolForKey:@"CayHasLaunchedBefore"]) return;

    askedFirstRunAutoStart = YES;

    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Cay - Lần chạy đầu tiên"];
    [alert setInformativeText:@"Bạn có muốn Cay tự động chạy khi khởi động máy không?"];
    [alert addButtonWithTitle:@"Có"];
    [alert addButtonWithTitle:@"Không"];

    [NSApp activateIgnoringOtherApps:YES];

    if ([alert runModal] == NSAlertFirstButtonReturn) {
        if (statusItem.menu) {
            for (NSMenuItem *item in statusItem.menu.itemArray) {
                if (item.action == @selector(toggleAutoStart:)) {
                    if (item.state == NSControlStateValueOff) {
                        [self toggleAutoStart:item];
                    }
                    break;
                }
            }
        }
    }

    [defaults setBool:YES forKey:@"CayHasLaunchedBefore"];
}

- (void)showAbout {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Cay – Bộ gõ tiếng Việt Telex v1.0.1"];
    [alert setInformativeText:@"aa→â  aw→ă  dd→đ  ee→ê  oo→ô  ow→ơ  uw→ư\n"
                              @"s=sắc  f=huyền  r=hỏi  x=ngã  j=nặng\n\n"
                              @"License: GPL-3.0\nSource: github.com/tctvn/cay"];
    [alert runModal];
}

- (void)quitApp {
    [NSApp terminate:nil];
}

@end

AppDelegate *g_delegate = nil;

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        g_delegate = [[AppDelegate alloc] init];
        app.delegate = g_delegate;
        [app run];
    }
    return 0;
}
