/**
 * Types.hh
 * Created by kate on 8/5/2023.
 * */

#ifndef MIKOTO_TYPES_HH
#define MIKOTO_TYPES_HH

// C++ Standard Library
#include <cstdint>
#include <cstddef>
#include <vector>
#include <filesystem>

namespace Mikoto {
    using Path_T = std::filesystem::path;

    using Int8_T = std::int8_t;
    using Int16_T = std::int16_t;
    using Int32_T = std::int32_t;
    using Int64_T = std::int64_t;

    using UInt8_T = std::uint8_t;
    using UInt16_T = std::uint16_t;
    using UInt32_T = std::uint32_t;
    using UInt64_T = std::uint64_t;

    using UShort_T = unsigned short;
    using ULong_T = unsigned short;
    using ULongLong_T = unsigned long long;

    using Short_T = unsigned short;
    using Long_T = unsigned long;
    using LongLong_T = long long;

    using Size_T = std::size_t;
}

#endif // MIKOTO_TYPES_HH
