#pragma once

namespace CayIME {
class MacHookManager {
public:
    // Khởi tạo CGEventTap. Trả về false nếu chưa có quyền Accessibility
    // hoặc tap không tạo được. KHÔNG hiện dialog hệ thống.
    static bool Initialize();

    // Hỏi macOS hiện dialog "Cấp quyền Accessibility" (chỉ xuất hiện
    // 1 lần đầu vì macOS cache). Trả về trạng thái trust hiện tại.
    static bool RequestAccessibilityPrompt();

    // Trạng thái trust hiện tại của process (poll khi user cấp quyền).
    static bool IsAccessibilityTrusted();

    // Đã khởi tạo eventTap thành công hay chưa.
    static bool IsRunning();

    static void Shutdown();
    static void ResetEngine();
};
} // namespace CayIME
