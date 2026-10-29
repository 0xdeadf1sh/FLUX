#pragma once

#include <cstdint>

namespace flux::utils
{
    ///////////////////////////////////////////////////////////////////////////
    constexpr bool isPowerOf2(auto x)
    {
        return x > 0 && !(x & (x - 1));
    }

    ///////////////////////////////////////////////////////////////////////////
    constexpr uint32_t countBits(auto x)
    {
        uint32_t cnt{};

        for (; x; x &= (x - 1))
            ++cnt;

        return cnt;
    }
}
