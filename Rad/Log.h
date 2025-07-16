#pragma once
#include <Windows.h>

#include "SourceLocation.h"

inline HRESULT ToHRESULT(BOOL b)
{
    return b ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

#define CHECK(x) if (!(x)) RadLog(LOG_ASSERT, TEXT(#x), SRC_LOC)
#define CHECK_RET(x,r) if (!(x)) { RadLog(LOG_ASSERT, TEXT(#x), SRC_LOC); return (r); }

#ifdef __cplusplus
#include "WinError.h"
#if 0
#define CHECK_LE(x) if (!(x)) { const DWORD err = GetLastError(); RadLog(LOG_ASSERT, WinError::getMessage(err, nullptr, TEXT(#x)), SRC_LOC); SetLastError(err); }
#define CHECK_LE_RET(x, r) if (!(x)) { const DWORD err = GetLastError(); RadLog(LOG_ASSERT, WinError::getMessage(err, nullptr, TEXT(#x)), SRC_LOC); SetLastError(err); return (r); }
#else
#define CHECK_LE(x) CheckHr(ToHRESULT(x), nullptr, TEXT(#x), SRC_LOC)
#define CHECK_LE_RET(x, r) if (FAILED(CheckHr(ToHRESULT(x), nullptr, TEXT(#x), SRC_LOC))) return (r);
#endif
#define CHECK_HR(x) CheckHr(x, nullptr, TEXT(#x), SRC_LOC)
#define CHECK_HR_RET(x, r) if (FAILED(CheckHr(x, nullptr, TEXT(#x), SRC_LOC))) return (r);
#endif

enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_ASSERT,
};

#ifdef __cplusplus
extern "C" {
#endif
    void RadLogInitWnd(HWND hWndLog, LPCSTR strLogCaptionA, LPCWSTR strLogCaptionW);
    void RadLogA(enum LogLevel l, const char* msg, SrcLocA src);
    void RadLogW(enum LogLevel l, const wchar_t* msg, SrcLocW src);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
#include <string>
inline void RadLog(LogLevel l, const char* msg, SrcLocA src) { RadLogA(l, msg, src); }
inline void RadLog(LogLevel l, const wchar_t* msg, SrcLocW src) { RadLogW(l, msg, src); }
inline void RadLog(LogLevel l, const std::string& msg, SrcLocA src) { RadLogA(l, msg.c_str(), src); }
inline void RadLog(LogLevel l, const std::wstring& msg, SrcLocW src) { RadLogW(l, msg.c_str(), src); }
#else
#ifdef _UNICODE
#define RadLog RadLogW
#else
#define RadLog RadLogA
#endif
#endif

#ifdef __cplusplus
inline DWORD CheckHr(const HRESULT hr, LPCSTR szModule, LPCSTR szContext, const SrcLocA src)
{
    if (FAILED(hr))
        RadLog(LOG_ASSERT, WinError::getMessage(hr, szModule, szContext), src);
    return hr;
}

inline DWORD CheckHr(const HRESULT hr, LPCWSTR szModule, LPCWSTR szContext, const SrcLocW src)
{
    if (FAILED(hr))
        RadLog(LOG_ASSERT, WinError::getMessage(hr, szModule, szContext), src);
    return hr;
}
#endif
