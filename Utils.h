#pragma once
#include <Windows.h>
#include <pdh.h>
#include <cmath>

#include "Rad/Log.h"
#include "Rad/WinError.h"

#define CHECK_HR_PDH(x) CheckHr(x, TEXT("pdh.dll"), TEXT(#x), SRC_LOC)
#define CHECK_HR_MSG_PDH(x, m) CheckHr(x, TEXT("pdh.dll"), m, SRC_LOC)

#include <system_error>

// bug in std::system_category().message() in MSVC v143
// See https://github.com/microsoft/STL/issues/3254

struct pdh_error_code
{
    explicit pdh_error_code(PDH_STATUS e) noexcept : error(e) {}
    PDH_STATUS error;
};

namespace std
{
    template <>
    struct is_error_code_enum<pdh_error_code> : std::true_type {};
}

_NODISCARD std::error_category& pdh_category() noexcept;

_NODISCARD inline std::error_code make_error_code(const pdh_error_code & wec)
{
    return std::error_code(static_cast<int>(wec.error), pdh_category());
}

_NODISCARD inline std::error_code make_pdh_error_code(PDH_STATUS ec) noexcept
{
    return std::error_code(pdh_error_code(ec));
}

[[noreturn]] inline void throw_pdh_error(PDH_STATUS ec)
{
    throw std::system_error(pdh_error_code(ec));
}

[[noreturn]] inline void throw_pdh_error(PDH_STATUS ec, const std::string & msg)
{
    throw std::system_error(pdh_error_code(ec), msg);
}

[[noreturn]] inline void throw_pdh_error(PDH_STATUS ec, const std::wstring & msg)
{
    throw_pdh_error(ec, w2a(msg));
}

[[noreturn]] inline void throw_pdh_error(PDH_STATUS ec, const char* msg)
{
    throw std::system_error(pdh_error_code(ec), msg);
}

[[noreturn]] inline void throw_pdh_error(PDH_STATUS ec, const wchar_t* msg)
{
    throw_pdh_error(ec, w2a(msg).c_str());
}

#define CHECK_HR_PDH_THROW(x) CheckHrThrow(pdh_error_code(x), TEXT(#x))

inline LONG Ignore(LONG e, std::initializer_list<LONG> ignored)
{
    for (LONG candidate : ignored)
    {
        if (e == candidate)
            return S_OK;
    }
    return e;
}

inline LONG Width(const RECT& r)
{
    return r.right - r.left;
}

inline LONG Height(const RECT& r)
{
    return r.bottom - r.top;
}

inline LSTATUS RegGetSZ(HKEY hKey, LPCSTR lpValue, LPSTR lpData, DWORD DataSize)
{
    DataSize *= sizeof(TCHAR);
    return CHECK_HR(RegGetValueA(hKey, nullptr, lpValue, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, lpData, &DataSize));
}

inline LSTATUS RegGetSZ(HKEY hKey, LPCWSTR lpValue, LPWSTR lpData, DWORD DataSize)
{
    DataSize *= sizeof(TCHAR);
    return CHECK_HR(RegGetValueW(hKey, nullptr, lpValue, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, lpData, &DataSize));
}

inline LSTATUS RegGetMULTISZ(HKEY hKey, LPCTSTR lpValue, LPTSTR lpData, DWORD DataSize)
{
    DataSize *= sizeof(TCHAR);
    return CHECK_HR(RegGetValue(hKey, nullptr, lpValue, RRF_RT_REG_MULTI_SZ, nullptr, lpData, &DataSize));
}

inline DWORD RegGetDWORD(HKEY hKey, LPCTSTR lpValue, DWORD Value)
{
    DWORD DataSize = sizeof(DWORD);
    CHECK_HR(RegGetValue(hKey, nullptr, lpValue, RRF_RT_DWORD, nullptr, &Value, &DataSize));
    return Value;
}

typedef unsigned __int64 QWORD;
inline QWORD RegGetQWORD(HKEY hKey, LPCTSTR lpValue, QWORD Value)
{
    DWORD DataSize = sizeof(QWORD);
    CHECK_HR(RegGetValue(hKey, nullptr, lpValue, RRF_RT_DWORD | RRF_RT_QWORD, nullptr, &Value, &DataSize));
    return Value;
}

inline double ceil_to_n_digits(double x, int n)
{
    if (x == 0)
        return x;

    const double scale = pow(10.0, ceil(log10(fabs(x))) - n);
    return ceil(x / scale) * scale;
}

inline int lerp(int a, int b, float t)
{
    return std::lround(a + t * (b - a));
}

inline COLORREF MakeLighter(COLORREF c, float t)
{
    BYTE r = lerp(GetRValue(c), 255, t);
    BYTE g = lerp(GetGValue(c), 255, t);
    BYTE b = lerp(GetBValue(c), 255, t);
    return RGB(r, g, b);
}

inline COLORREF MakeDarker(COLORREF c, float t)
{
    BYTE r = lerp(GetRValue(c), 0, t);
    BYTE g = lerp(GetGValue(c), 0, t);
    BYTE b = lerp(GetBValue(c), 0, t);
    return RGB(r, g, b);
}
