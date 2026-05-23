# 🌶️ Cay — Bộ gõ Telex Minimalist

[![Release](https://img.shields.io/github/v/release/tctvn/cay?style=flat-square&color=FF4500)](https://github.com/tctvn/cay/releases)
[![Size](https://img.shields.io/badge/size-22_KB-brightgreen?style=flat-square)](https://github.com/tctvn/cay/releases/download/cay/cay.exe)
[![Platform](https://img.shields.io/badge/platform-Windows-0078d7?style=flat-square)](https://github.com/tctvn/cay/releases)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue?style=flat-square)](LICENSE)

Bộ gõ Telex siêu nhẹ, "plug-and-play" dành cho anh em developer hệ tối giản. Không config rườm rà, bật lên là gõ với trải nghiệm mượt mà, được thiết kế để hạn chế tối đa việc phá hỏng code hay lỗi vặt trên IDE.

➡️ [**Download cay.exe (22 KB)**](https://github.com/tctvn/cay/releases/download/cay/cay.exe)

---

## ✨ Tính năng "ăn tiền"

- **Siêu nhẹ & Sạch sẽ:** File thực thi chỉ vỏn vẹn ~22KB. Chạy độc lập, không phụ thuộc thư viện rườm rà, ngốn RAM gần như bằng 0.
- **Không lỗi vặt:** Đã fix triệt để các bệnh nan y như lỗi nuốt chữ, nhảy nháy khi gõ trên thanh địa chỉ Chrome, thanh tìm kiếm VSCode hay GitHub/CodeMirror.
- **Smart Bypass (Gõ code cực bốc):** Tự động nhận diện từ tiếng Anh hoặc các chuỗi sai quy tắc chính tả tiếng Việt để nhường lại quyền gõ phím nguyên thủy. Được nâng cấp với thuật toán **Hard Filter (Luật phụ âm kép)**, ngắt ngay các từ tiếng Anh như `style`, `class` từ ký tự thứ 2, giúp gõ code cực mượt và tiết kiệm tối đa CPU cycle.
- **Kiến trúc đa nền tảng (Cross-platform ready):** Lõi xử lý Telex được tách biệt hoàn toàn khỏi Windows API, không dùng STL, không cấp phát động. Sẵn sàng port sang macOS/Linux mà vẫn giữ nguyên bản sắc siêu nhẹ gọn.
- **Tinh gọn tối đa (Zero Bloat):** Mạnh tay cắt bỏ toàn bộ những tính năng "thừa thãi" mà 99% người dùng không bao giờ đụng tới ở các bộ gõ khác (bảng mã TCVN3/VNI cũ rích, kiểu gõ VIQR, macro gõ tắt, giao diện rườm rà, tool chuyển mã ). Chỉ tập trung duy nhất vào **Unicode** và **Telex** để đạt hiệu năng tối thượng.
- **Zero-config:** Không cần chỉnh bảng mã, không cần chọn kiểu gõ. Tải về là chạy, bật là gõ.
## 🛠️ Build từ Source

Yêu cầu: Có sẵn `CMake` và `MSVC`.

```bash
git clone https://github.com/tctvn/cay.git
cd cay
cmake -B build
cmake --build build --config Release
```
*File build xong sẽ nằm gọn tại `build/Release/cay.exe`.*

## ⌨️ Cách dùng

- Chạy file `cay.exe` (app sẽ chạy ngầm dưới khay hệ thống - System Tray).
- Cú pháp chuẩn Telex: `aa`=â, `oo`=ô, `ee`=ê, `dd`=đ, `w`=ă/ư/ơ.
- Dấu: `s`=sắc, `f`=huyền, `r`=hỏi, `x`=ngã, `j`=nặng, `z`=xoá dấu.
- **Phím tắt Bật/Tắt:** `Ctrl + Shift`.

## 📜 License
[GPL-3.0 License](LICENSE) © [tctvn](https://github.com/tctvn/cay).
Mã nguồn: [GitHub Repository](https://github.com/tctvn/cay).
