#include "Utils.h"

std::error_category& pdh_category() noexcept
{
    static pdh_error_category pdhec;
    return pdhec;
}
