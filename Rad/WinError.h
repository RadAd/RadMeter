#pragma once
#include <Windows.h>
#include <string>

#include "Convert.h"

#ifdef _UNICODE
#define tstring wstring
#else
#define tstring string
#endif

struct WinError
{
    DWORD dwError;
    LPCTSTR szModule;
    std::tstring szContext;

    std::tstring getMessage() const
    {
        return getMessage(dwError, szModule, szContext.c_str());
    }

    static std::string getMessage(DWORD dwError, LPCSTR szModule, LPCSTR szContext);
    static std::wstring getMessage(DWORD dwError, LPCWSTR szModule, LPCWSTR szContext);
    static std::string getMessage(HRESULT hr, LPCSTR szModule, LPCSTR szContext);
    static std::wstring getMessage(HRESULT hr, LPCWSTR szModule, LPCWSTR szContext);
};

#include <system_error>

// bug in std::system_category().message() in MSVC v143
// See https://github.com/microsoft/STL/issues/3254

struct win32_error_code
{
    explicit win32_error_code(DWORD e) noexcept : error(e) {}
    DWORD error;
};

namespace std
{
    template <>
    struct is_error_code_enum<win32_error_code> : std::true_type {};
}

_NODISCARD std::error_category& win32_category() noexcept;

_NODISCARD inline std::error_code make_error_code(const win32_error_code& wec)
{
    return std::error_code(static_cast<int>(wec.error), win32_category());
}

_NODISCARD inline std::error_code make_win32_error_code(DWORD ec = GetLastError()) noexcept
{
    return std::error_code(win32_error_code(ec));
}

[[noreturn]] inline void throw_win32_error(DWORD ec = GetLastError())
{
    throw std::system_error(win32_error_code(ec));
}

[[noreturn]] inline void throw_win32_error(DWORD ec, const std::string& msg)
{
    throw std::system_error(win32_error_code(ec), msg);
}

[[noreturn]] inline void throw_win32_error(DWORD ec, const std::wstring& msg)
{
    throw_win32_error(ec, w2a(msg));
}

[[noreturn]] inline void throw_win32_error(DWORD ec, const char* msg)
{
    throw std::system_error(win32_error_code(ec), msg);
}

[[noreturn]] inline void throw_win32_error(DWORD ec, const wchar_t* msg)
{
    throw_win32_error(ec, w2a(msg).c_str());
}

struct hr_error_code
{
    explicit hr_error_code(HRESULT e) noexcept : error(e) {}
    HRESULT error;
};

namespace std
{
    template <>
    struct is_error_code_enum<hr_error_code> : std::true_type {};
}

_NODISCARD std::error_category& hr_category() noexcept;

_NODISCARD inline std::error_code make_error_code(const hr_error_code& wec)
{
    return std::error_code(static_cast<int>(wec.error), hr_category());
}

_NODISCARD inline std::error_code make_hr_error_code(HRESULT ec) noexcept
{
    return std::error_code(hr_error_code(ec));
}

[[noreturn]] inline void throw_hr_error(HRESULT ec)
{
    throw std::system_error(hr_error_code(ec));
}

[[noreturn]] inline void throw_hr_error(HRESULT ec, const std::string& msg)
{
    throw std::system_error(hr_error_code(ec), msg);
}

[[noreturn]] inline void throw_hr_error(HRESULT ec, const std::wstring& msg)
{
    throw_hr_error(ec, w2a(msg));
}

[[noreturn]] inline void throw_hr_error(HRESULT ec, const char* msg)
{
    throw std::system_error(hr_error_code(ec), msg);
}

[[noreturn]] inline void throw_hr_error(HRESULT ec, const wchar_t* msg)
{
    throw_hr_error(ec, w2a(msg).c_str());
}

inline void CheckHrThrow(const HRESULT hr, const char* msg)
{
    if (FAILED(hr))
        throw_hr_error(hr, msg);
}

inline void CheckHrThrow(const HRESULT hr, const wchar_t* msg)
{
    if (FAILED(hr))
        throw_hr_error(hr, msg);
}

//#define CHECK_LE_THROW(x) if (!(x)) throw WinError({ GetLastError(), nullptr, TEXT(#x) })
#if 0
#define CHECK_LE_THROW(x) if (!(x)) throw_win32_error(GetLastError(), TEXT(#x))
#else
#define CHECK_LE_THROW(x) CheckHrThrow(ToHRESULT(x), TEXT(#x))
#endif
#define CHECK_HR_THROW(x) CheckHrThrow(x, TEXT(#x))
