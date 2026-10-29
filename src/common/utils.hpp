#pragma once

namespace flux::utils
{
    ///////////////////////////////////////////////////////////////////////////
    constexpr bool isPowerOf2(auto x)
    {
        return x > 0 && !(x & (x - 1));
    }
}
