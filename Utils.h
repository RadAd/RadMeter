#pragma once
#include <Windows.h>
#include <pdh.h>
#include <tchar.h>
#include <cmath>

#include "Rad/Format.h"
#include "Rad/WinError.h"

inline DWORD CheckThrow(DWORD status, LPCTSTR szModule = nullptr)
{
    if (FAILED(status))
        throw WinError({ status, szModule });
    return status;
}

inline DWORD CheckLog(DWORD status, const std::wstring& context, LPCTSTR szModule = nullptr)
{
    if (FAILED(status))
    {
        WinError e({ status, szModule });
        OutputDebugString((context + _T(": ") + e.getMessage() + _T('\n')).c_str());
    }
    return status;
}

inline PDH_STATUS CheckPdhThrow(PDH_STATUS status)
{
    return CheckThrow(status, _T("pdh.dll"));
}

inline PDH_STATUS CheckPdhLog(PDH_STATUS status, const std::wstring& context)
{
    return CheckLog(status, context, _T("pdh.dll"));
}

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
    return CheckLog(RegGetValueA(hKey, nullptr, lpValue, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, lpData, &DataSize), _T("RegGetSZ"));
}

inline LSTATUS RegGetSZ(HKEY hKey, LPCWSTR lpValue, LPWSTR lpData, DWORD DataSize)
{
    DataSize *= sizeof(TCHAR);
    return CheckLog(RegGetValueW(hKey, nullptr, lpValue, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, lpData, &DataSize), _T("RegGetSZ"));
}

inline LSTATUS RegGetMULTISZ(HKEY hKey, LPCTSTR lpValue, LPTSTR lpData, DWORD DataSize)
{
    DataSize *= sizeof(TCHAR);
    return CheckLog(RegGetValue(hKey, nullptr, lpValue, RRF_RT_REG_MULTI_SZ, nullptr, lpData, &DataSize), _T("RegGetMULTISZ"));
}

inline DWORD RegGetDWORD(HKEY hKey, LPCTSTR lpValue, DWORD Value)
{
    DWORD DataSize = sizeof(DWORD);
    CheckLog(RegGetValue(hKey, nullptr, lpValue, RRF_RT_DWORD, nullptr, &Value, &DataSize), _T("RegGetDWORD"));
    return Value;
}

typedef unsigned __int64 QWORD;
inline QWORD RegGetQWORD(HKEY hKey, LPCTSTR lpValue, QWORD Value)
{
    DWORD DataSize = sizeof(QWORD);
    CheckLog(RegGetValue(hKey, nullptr, lpValue, RRF_RT_DWORD | RRF_RT_QWORD, nullptr, &Value, &DataSize), _T("RegGetQWORD"));
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
