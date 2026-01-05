/**
 * Random.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_RANDOM_HH
#define MIKOTO_RANDOM_HH

// C++ Standard Library
#include <random>
#include <stdexcept>
#include <limits>
#include <vector>
#include <unordered_set>

// Third-Party Library
#include "fmt/format.h"

// Project Headers
#include <Common/Common.hh>
#include <Core/Exception.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    /**
     * Returns a seed for random number generation
     * @returns seed
     * */
    MKT_NODISCARD inline auto GetSeed() -> std::random_device& {
        static std::random_device seed{};
        return seed;
    }


    /**
     * Returns a random 64-bit integer
     * @return random 64-bit integer
     * */
    MKT_NODISCARD inline auto GetRandomUInt64(std::random_device& seed = GetSeed()) -> UInt64 {
        thread_local std::mt19937_64 mt{ seed() };

        return std::uniform_int_distribution<UInt64>{
                (std::numeric_limits<UInt64>::min)(),
                (std::numeric_limits<UInt64>::max)(),
        }(mt);
    }


    /**
     * Returns a random integer
     * @return random integer
     * */
    MKT_NODISCARD inline auto GetRandomUInt32(std::random_device& seed = GetSeed()) -> UInt32 {
        thread_local std::mt19937 mt{ seed() };

        return std::uniform_int_distribution<UInt32>{
                (std::numeric_limits<UInt32>::min)(),
            (std::numeric_limits<UInt32>::max)(),
        }(mt);
    }


    /**
     * Returns a random integer in the range given by the parameters.
     * Throws if the range is ill-formed.
     * @return random integer in the range [lowerBound, upperbound)
     * @throws std::runtime_error if the range is invalid
     * */
    MKT_NODISCARD inline auto GetRandomInt(Int32 lowerBound, Int32 upperbound, std::random_device& seed = GetSeed()) -> Int32 {
        if (lowerBound > upperbound) {
            MKT_THROW_RUNTIME_ERROR(fmt::format("Invalid range for random integer generation. Lower bound is {}, upperbound is {}", lowerBound, upperbound));
        }

        thread_local std::mt19937 mt{ seed() };
        return std::uniform_int_distribution{ lowerBound, upperbound }(mt);
    }


    /**
     * Returns a random real number in the range given by the parameters.
     * Throws if the range is ill-formed
     * @return random integer in the range [lowerBound, upperbound)
     * @throws std::runtime_error if the range is invalid
     * */
    MKT_NODISCARD inline auto GetRandomReal(double lowerBound, double upperbound, std::random_device& seed = GetSeed()) -> double {
        if (lowerBound > upperbound) {
            MKT_THROW_RUNTIME_ERROR(fmt::format("Invalid range for random integer generation. Lower bound is {}, upperbound is {}", lowerBound, upperbound));
        }

        thread_local std::mt19937 mt{ seed() };
        return std::uniform_real_distribution{ lowerBound, upperbound }(mt);
    }


    MKT_NODISCARD inline auto GetRandomIntNumberList(Size length, Int32 lowerBound = -1000, Int32 upperbound = 1000, std::random_device& seed = GetSeed()) -> std::vector<Int32> {
        std::vector<Int32> result{};

        Size count{};
        result.reserve(length);
        for ( ; count < length; ++count)
            result.emplace_back(GetRandomInt(lowerBound, upperbound, seed));

        return result;
    }


    MKT_NODISCARD inline auto GetRandomRealNumberList(Size length, double lowerBound = -1000.0, double upperbound = 1000.0, std::random_device& seed = GetSeed()) -> std::vector<double> {
        std::vector<double> result{};

        Size count{};
        result.reserve(length);
        for ( ; count < length; ++count)
            result.emplace_back(GetRandomReal(lowerBound, upperbound, seed));

        return result;
    }


    inline auto GetGUIDs() -> std::unordered_set<UInt64>& {
        static std::unordered_set<UInt64> guids{};

        return guids;
    }

    /**
     * Inserts guid to the list of valid guids
     * @param guid new guid
     * */
    inline auto ValidateGUID(UInt64 guid) -> void {
        GetGUIDs().emplace(guid);
    }

    /**
     * Removes guid from the list of valid guids
     * @param guid id to be removed
     * */
    inline auto InvalidateGUID(UInt64 guid) -> void {
        GetGUIDs().erase(guid);
    }

    /**
     * Returns a seed for random number generation
     * @returns seed
     * */
    MKT_NODISCARD inline auto GetGUIDSeed() -> std::random_device& {
        static std::random_device seed{};
        return seed;
    }

    /**
     * Returns true if a guid has not been generated yet, false otherwise
     * @returns if a guid is valid or not
     * */
    MKT_NODISCARD inline auto IsUse(UInt64 guid) -> bool {
        return GetGUIDs().contains(guid);
    }

    /**
     * Returns a unique 64-bit integer every time it is called. This
     * function is used for the purpose of GUID's
     * @returns universally unique integer
     * */
    MKT_NODISCARD inline auto GenerateGUID() -> UInt64 {
        static auto& seed{ GetGUIDSeed() };

        UInt64 result{ 0 };

        do {
            result = GetRandomUInt64(seed);
        }
        while(IsUse(result));

        ValidateGUID(result);

        return result;
    }

    class GlobalUniqueID {
    public:
        explicit GlobalUniqueID()
            :   m_Id{  GenerateGUID() }
        {
        }

        explicit operator UInt64 () const { return m_Id; }

        MKT_NODISCARD auto Get() const -> UInt64 { return m_Id; }

        ~GlobalUniqueID() { InvalidateGUID(m_Id); }

    private:
        UInt64 m_Id{};
    };
}
#endif // MIKOTO_RANDOM_HH
