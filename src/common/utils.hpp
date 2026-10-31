#pragma once

#include <vector>
#include <string_view>
#include <fstream>
#include <format>
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

    ///////////////////////////////////////////////////////////////////////////
    inline std::vector<char> readBinaryFile(std::string_view filename)
    {
        if (std::ifstream fp{ filename.cbegin(), std::ios::ate | std::ios::binary })
        {
            std::streamoff tell{ fp.tellg() };
            if (tell < 0)
            {
                throw std::runtime_error
                {
                    std::format("tellg() failed when reading binary '{}'", filename)
                };
            }
            else if (tell == 0)
            {
                throw std::runtime_error
                {
                    std::format("binary '{}' is empty", filename)
                };
            }

            size_t fileSize{ static_cast<size_t>(tell) };
            std::vector<char> buffer(fileSize);

            fp.seekg(0, std::ios::beg);
            fp.read(&buffer[0], tell);

            fp.close();
            return buffer;
        }

        throw std::runtime_error
        {
            std::format("couldn't read binary '{}'", filename)
        };
    }
}
