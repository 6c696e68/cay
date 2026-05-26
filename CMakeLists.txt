cmake_minimum_required(VERSION 3.20)
project(cay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build for both Apple Silicon and Intel" FORCE)
    enable_language(OBJCXX)
endif()

# ---------------------------------------------------------------------------
# Core Sources
# ---------------------------------------------------------------------------
set(CORE_SOURCES
    src/core/CayData.cpp
    src/core/CayEngine.cpp
)

# ---------------------------------------------------------------------------
# OS-Specific setup
# ---------------------------------------------------------------------------
if(WIN32)
    set(PLATFORM_SOURCES
        src/platform/windows/main.cpp
        src/platform/windows/InputInjector.cpp
        src/platform/windows/KeyboardHookManager.cpp
        src/platform/windows/no_crt.cpp
    )
    add_executable(cay WIN32 ${CORE_SOURCES} ${PLATFORM_SOURCES})
    target_include_directories(cay PRIVATE src/core src/platform/windows)

    # Strip CMake-injected /EHsc and /GR so we can override them cleanly
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/GR"   "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    target_compile_options(cay PRIVATE
        /W3          # Warning level 3
        /O1          # Optimise for size
        /Os          # Favour small code
        /GS-         # Disable buffer security check
        /EHs-c-      # Disable C++ exceptions
        /GR-         # Disable RTTI
        /Gy          # Function-level linking (COMDAT)
        /MT          # Static CRT → tells headers to use non-import signatures
        /DUNICODE /D_UNICODE
        /DWIN32 /D_WINDOWS
        /DNDEBUG
    )

    target_link_options(cay PRIVATE
        # Exclude dynamic and debug CRT variants only
        /NODEFAULTLIB:msvcrt.lib
        /NODEFAULTLIB:msvcrtd.lib
        /NODEFAULTLIB:libcmtd.lib
        /NODEFAULTLIB:vcruntimed.lib
        /NODEFAULTLIB:ucrtd.lib
        /ENTRY:wWinMain          # Our GUI entry point
        /SUBSYSTEM:WINDOWS
        /OPT:REF                 # Strip unreferenced functions/data
        /OPT:ICF                 # Merge identical COMDATs
        /MANIFEST:NO             # No UAC manifest
        /machine:x64
    )

    # Win32 API + static CRT libraries (explicit list, no auto-link ambiguity)
    target_link_libraries(cay PRIVATE
        kernel32.lib
        user32.lib
        gdi32.lib
        shell32.lib
        advapi32.lib
        libcmt.lib        # Static CRT – wcslen, wcscpy_s, printf family
        libvcruntime.lib  # CRT helpers – __chkstk, memset, memcpy etc.
        libucrt.lib       # Universal CRT – wide-char, math etc.
    )

elseif(APPLE)
    set(PLATFORM_SOURCES
        src/platform/macos/AppDelegate.mm
        src/platform/macos/MacInputInjector.mm
        src/platform/macos/MacHookManager.mm
    )

    set_source_files_properties(
        ${PLATFORM_SOURCES}
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )
    
    # MACOSX_BUNDLE makes it an .app bundle
    add_executable(cay MACOSX_BUNDLE ${CORE_SOURCES} ${PLATFORM_SOURCES})
    target_include_directories(cay PRIVATE src/core src/platform/macos)

    # Bundle configuration
    set_target_properties(cay PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.tctvn.cay"
        MACOSX_BUNDLE_BUNDLE_NAME "Cay"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.6"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/macos/Info.plist.in
    )

    # Link frameworks (chỉ những framework thực sự dùng).
    # - Cocoa: NSAlert/NSStatusBar/NSWorkspace/NSTimer/NSTask/NSRunningApplication...
    # - CoreGraphics: CGEventTap/CGEventCreateKeyboardEvent (low-level event tap).
    # - ApplicationServices: AXIsProcessTrusted (Accessibility check).
    #
    # Đã bỏ Carbon (không có call site) và ServiceManagement (chỉ import header,
    # không gọi SMLoginItem/SMJobBless). Loại 2 framework này giảm số dyld
    # imports → khởi động nhanh hơn và linker dead_strip hiệu quả hơn.
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
    find_library(APPLICATIONSERVICES_FRAMEWORK ApplicationServices)

    target_link_libraries(cay PRIVATE
        ${COCOA_FRAMEWORK}
        ${COREGRAPHICS_FRAMEWORK}
        ${APPLICATIONSERVICES_FRAMEWORK}
    )

    # ---------------------------------------------------------------------
    # Compile + link flags cho size tối thiểu (macOS Release).
    # ---------------------------------------------------------------------
    target_compile_options(cay PRIVATE
        -Oz                              # Tối ưu size aggressive (nhỏ hơn -Os)
        -fno-exceptions                  # Không C++ exceptions
        -fno-rtti                        # Không RTTI
        -fno-unwind-tables               # Không unwind tables (no exceptions = no need)
        -fno-asynchronous-unwind-tables  # Không async unwind tables
        -fvisibility=hidden              # Symbol mặc định hidden
        -fvisibility-inlines-hidden      # Inline symbol hidden
        -fdata-sections                  # Mỗi data trong section riêng
        -ffunction-sections              # Mỗi function trong section riêng
        -fmerge-all-constants            # Merge constants giống nhau
        -fomit-frame-pointer             # Bỏ frame pointer (nhỏ hơn, ổn cho release)
        -flto                            # Link-time optimization (cross-TU dead-strip)
    )

    target_link_options(cay PRIVATE
        -Wl,-dead_strip                   # Strip unreferenced sections
        -Wl,-dead_strip_dylibs            # Strip unreferenced dylibs
        -Wl,-x                            # Strip non-global symbols
        -Wl,-S                            # Strip debug symbols
        -flto                             # LTO ở link stage
    )

    # Ad-hoc codesign sau build: bắt buộc cho TCC (Accessibility) trên Apple
    # Silicon, và giúp entry Accessibility ổn định giữa các lần rebuild
    # cùng bundle id.
    add_custom_command(TARGET cay POST_BUILD
        COMMAND ${CMAKE_STRIP} -x "$<TARGET_FILE:cay>"
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:cay>"
        COMMENT "Strip + ad-hoc codesigning cay.app"
        VERBATIM
    )

elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    # Old uinput code was removed by the user. Fcitx5 is now the primary Linux target.
endif()

# ---------------------------------------------------------------------------
# Fcitx5 Plugin (Optional on Linux)
# ---------------------------------------------------------------------------
option(BUILD_FCITX5 "Build Fcitx5 Plugin" OFF)
if(BUILD_FCITX5)
    add_subdirectory(src/platform/fcitx5)
endif()

# ---------------------------------------------------------------------------
# Property-Based Test target (Optional)
#
# Test binary `cay_test` được build độc lập với release binary `cay`.
# Test build CỐ Ý KHÔNG kế thừa các cờ no-CRT/no-exception/no-RTTI của
# Windows release (`/EHs-c-`, `/GR-`, `/NODEFAULTLIB:msvcrt.lib`) — test code
# được phép dùng STL (<vector>, <string>, <random>, doctest, ...) bình thường
# vì nó chạy trong môi trường developer/CI, không phải binary phân phối.
#
# Release binary `cay` giữ nguyên ràng buộc no-CRT trên Windows.
# ---------------------------------------------------------------------------
option(BUILD_TESTING "Build property-based tests" OFF)
if(BUILD_TESTING)
    enable_testing()

    # Gom test sources từ test/*.cpp. Tại Phase 0 các file này được tạo dần
    # bởi task 1.2 (property.h harness), 1.5 (golden_trace.cpp), 1.7 (test_*).
    # Vì vậy ta phải cho phép cấu hình thành công NGAY CẢ KHI chưa có file
    # test nào — chỉ tạo executable khi danh sách non-empty.
    file(GLOB CAY_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")

    if(CAY_TEST_SOURCES)
        add_executable(cay_test ${CORE_SOURCES} ${CAY_TEST_SOURCES})
        target_compile_definitions(cay_test PRIVATE CAY_TEST_BUILD)
        target_include_directories(cay_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core
            ${CMAKE_CURRENT_SOURCE_DIR}/test
        )
        add_test(NAME cay_test COMMAND cay_test)
    else()
        message(STATUS
            "BUILD_TESTING=ON nhưng chưa có file test/*.cpp nào — "
            "bỏ qua target cay_test (sẽ được tạo khi task 1.2/1.5/1.7 thêm test sources)."
        )
    endif()
endif()
