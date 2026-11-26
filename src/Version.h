#pragma once

#ifndef CR3_VER_MAJOR
#define CR3_VER_MAJOR 1
#endif
#ifndef CR3_VER_MINOR
#define CR3_VER_MINOR 1
#endif
#ifndef CR3_VER_PATCH
#define CR3_VER_PATCH 0
#endif

#define CR3_STR_HELPER(x) #x
#define CR3_STR(x) CR3_STR_HELPER(x)
#define CR3_PASTE(a, b) a##b
#define CR3_WIDEN(s) CR3_PASTE(L, s)
#define CR3_WSTR(x) CR3_WIDEN(CR3_STR(x))

#define CR3_VER_STR_A CR3_STR(CR3_VER_MAJOR) "." CR3_STR(CR3_VER_MINOR) "." CR3_STR(CR3_VER_PATCH) ".0"
#define CR3_VER_STR_W CR3_WSTR(CR3_VER_MAJOR) L"." CR3_WSTR(CR3_VER_MINOR) L"." CR3_WSTR(CR3_VER_PATCH) L".0"
