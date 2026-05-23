// no_crt.cpp
// Provides the bare minimum symbols needed to replace the CRT when building
// with /NODEFAULTLIB.  All implementations use Win32 Heap API only.
//
// wcslen / wcscpy_s / wcscat / wmemcmp  -> resolved from ntdll.lib
// memset / memcpy                       -> resolved from ntdll.lib (RtlFillMemory etc.)
//
// This file only supplies C++ operator new / delete.

#include <windows.h>

// ---------------------------------------------------------------------------
// Heap-based operator new / delete (no CRT)
// ---------------------------------------------------------------------------
void* __cdecl operator new(size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
}

void* __cdecl operator new[](size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
}

void __cdecl operator delete(void* ptr) noexcept {
    if (ptr) HeapFree(GetProcessHeap(), 0, ptr);
}

void __cdecl operator delete[](void* ptr) noexcept {
    if (ptr) HeapFree(GetProcessHeap(), 0, ptr);
}

// Sized delete overloads (required by C++17 ABI)
void __cdecl operator delete(void* ptr, size_t) noexcept {
    if (ptr) HeapFree(GetProcessHeap(), 0, ptr);
}

void __cdecl operator delete[](void* ptr, size_t) noexcept {
    if (ptr) HeapFree(GetProcessHeap(), 0, ptr);
}

extern "C" {
    #pragma function(memset)
    void* __cdecl memset(void* dest, int c, size_t count) {
        char* bytes = (char*)dest;
        while (count--) *bytes++ = (char)c;
        return dest;
    }

    #pragma function(memcpy)
    void* __cdecl memcpy(void* dest, const void* src, size_t count) {
        char* d = (char*)dest;
        const char* s = (const char*)src;
        while (count--) *d++ = *s++;
        return dest;
    }
}

