# 🌶️ Cay — Bộ gõ Telex Siêu Cứng Cho Developer

[![Release](https://img.shields.io/github/v/release/tctvn/cay?style=flat-square&color=FF4500)](https://github.com/tctvn/cay/releases)
[![Size](https://img.shields.io/badge/size-22_KB-brightgreen?style=flat-square)](https://github.com/tctvn/cay/releases/download/cay/cay.exe)
[![Platform](https://img.shields.io/badge/platform-Windows-0078d7?style=flat-square)](https://github.com/tctvn/cay/releases)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue?style=flat-square)](LICENSE)

Bộ gõ Telex "nhỏ mà có võ", chỉ vỏn vẹn **22 KB** (nhỏ hơn cả một bức ảnh mờ mịt của người yêu cũ). Được thiết kế đặc biệt dành riêng cho anh em coder hệ tối giản, ghét sự rườm rà, căm thù cái lỗi nhảy nháy con trỏ mỗi khi gõ code. 

Bật là gõ, không lằng nhằng!

➡️ [**Tải ngay cay.exe (22 KB) tại đây**](https://github.com/tctvn/cay/releases/download/cay/cay.exe)

---

## ✨ Những "Cú Đấm" Ăn Tiền Của Cay

- 🚀 **Nhỏ Gọn Vô Địch (Zero-CRT):** Viết bằng C++ thuần túy, ép xung biên dịch cắt bỏ hoàn toàn C-Runtime và thư viện rác. Dung lượng siêu ảo chỉ **22KB**, ăn RAM gần như bằng 0.
- 🛠️ **Sạch Sẽ, Không Lỗi Vặt:** Fix triệt để các căn bệnh nan y ung thư tủy như: lỗi nuốt chữ, nhảy con trỏ khi gõ trên Chrome Omnibox, Excel, hay các editor khó tính như CodeMirror/GitHub/VSCode.
- 🧠 **Smart Bypass (Gõ Code Cực Bốc):** Thuật toán tự động nhận diện từ tiếng Anh siêu thông minh để nhường lại quyền gõ phím gốc. Bạn cứ gõ code thoải mái mà không lo bị dính dấu tiếng Việt.
- ⚡ **Hard Filter (Luật Phụ Âm Kép):** Cải tiến tối thượng! Nhận dạng các từ tiếng Anh (như `style`, `class`, `block`, ...) ngay từ **ký tự thứ 2**. Vô hiệu hoá bộ gõ ngay lập tức giúp tiết kiệm tối đa CPU cycle và triệt tiêu độ trễ!
- 🌐 **Kiến Trúc Core Đa Nền Tảng:** Lõi xử lý Telex được cô lập hoàn toàn khỏi Windows API, không dùng STL, không cấp phát động. Sẵn sàng đem đi chinh phạt macOS/Linux.
- 🗑️ **Zero-Bloat:** Mạnh tay "cắt phăng" toàn bộ những tính năng thừa thãi mà 99% người dùng không bao giờ xài (bảng mã cổ đại TCVN3/VNI, kiểu gõ VIQR, macro gõ tắt, tool chuyển mã). Chỉ tập trung làm cực tốt 2 thứ: **Unicode** và **Telex**.
- ⚙️ **Zero-Config:** Tải về, nhấp đúp là chạy. Không cần cài đặt, không cần tuỳ chỉnh.

---

## ⌨️ Cách Xài

- **Chạy:** Nhấp đúp `cay.exe`, icon chữ V đỏ chót sẽ nằm chờ sẵn dưới System Tray.
- **Cú pháp Telex chuẩn:** `aa`=â, `oo`=ô, `ee`=ê, `dd`=đ, `w`=ă/ư/ơ.
- **Dấu:** `s`=sắc, `f`=huyền, `r`=hỏi, `x`=ngã, `j`=nặng, `z`=xoá dấu.
- **Tắt/Bật nhanh:** Nhấn tổ hợp `Ctrl + Shift`.

---

## 🛠️ Build Từ Source Code

Anh em nào thích vọc vạch, tự build tự sướng thì cần có `CMake` và `MSVC`. (Code sạch đẹp, kiến trúc chia `core` và `platform` cực chuẩn chỉnh).

```bash
git clone https://github.com/tctvn/cay.git
cd cay
cmake -B build
cmake --build build --config Release
```
*File build ngon lành cành đào sẽ nằm ở: `build/Release/cay.exe`.*

---

## 📜 Giấy Phép & Bản Quyền
- **Mã nguồn:** [GitHub Repository](https://github.com/tctvn/cay)
- Giấy phép mã nguồn mở [GPL-3.0 License](LICENSE) © [tctvn](https://github.com/tctvn/cay).
