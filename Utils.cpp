#include "Utils.h"

class pdh_error_category : public std::error_category
{
public:
    constexpr pdh_error_category() noexcept : std::error_category(_Generic_addr) {}

    _NODISCARD const char* name() const noexcept override
    {
        return "pdh";
    }

    _NODISCARD std::string message(int errcode) const override
    {
        return WinError::getMessage(PDH_STATUS(errcode), "pdh.dll", LPCSTR(nullptr));
    }
};

std::error_category& pdh_category() noexcept
{
    static pdh_error_category pdhec;
    return pdhec;
}
