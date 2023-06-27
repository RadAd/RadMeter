#pragma once
#include <Windows.h>
#include <pdh.h>
#include <memory>
#include <tchar.h>
#include <cmath>

#include "Format.h"

struct QueryDeleter
{
    typedef PDH_HQUERY pointer;
    constexpr void operator()(PDH_HQUERY arg) const
    {
        if (arg != NULL)
            PdhCloseQuery(arg);
    }
};

struct LocalDeleter
{
    typedef LPTSTR pointer;
    constexpr void operator()(LPTSTR arg) const
    {
        if (arg != NULL)
            LocalFree(arg);
    }
};

struct KeyDeleter
{
    typedef HKEY pointer;
    constexpr void operator()(HKEY arg) const
    {
        if (arg != NULL)
            RegCloseKey(arg);
    }
};

template <class T>
class out_ptr
{
public:
    typedef typename T::pointer pointer;

    out_ptr(T& t)
        : tt(t)
    {
    }

    operator pointer*()
    {
        return &pp;
    }

    pointer* get()
    {
        return &pp;
    }

    ~out_ptr()
    {
        tt.reset(pp);
    }

private:
    T& tt;
    pointer pp = {};
};

struct WinError
{
    DWORD dwError;
    LPCTSTR szModule;
    std::wstring szContext;

    std::wstring getMessage() const
    {
        HMODULE hLibrary = szModule != nullptr ? GetModuleHandle(szModule) : nullptr;
        std::unique_ptr<WCHAR[], LocalDeleter> pMessage;
        if (!FormatMessage(FORMAT_MESSAGE_FROM_HMODULE |
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            hLibrary,
            dwError,
            0,
            (LPTSTR) out_ptr(pMessage).get(),
            0,
            NULL))
        {
            return Format(_T("Format message failed with 0x%x"), GetLastError());
        }
        else
        {
            pMessage[_tcslen(pMessage.get()) - 2] = _T('\0');
            return Format(_T("Error: %s 0x%x %s"), szContext.c_str(), dwError, pMessage.get());
        }
    }
};

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
