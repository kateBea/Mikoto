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
#include <fmt/format.h>

// Project Headers
#include <Utility/Types.hh>
#include <Utility/Common.hh>
#include <Core/Logger.hh>

namespace Mikoto::Random {
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
    MKT_NODISCARD inline auto GetRandomUInt64(auto& seed = GetSeed()) -> UInt64_T {
        thread_local std::mt19937_64 mt{ seed() };

        return std::uniform_int_distribution<UInt64_T>{
                std::numeric_limits<UInt64_T>::min(),
                std::numeric_limits<UInt64_T>::max(),
        }(mt);
    }

    /**
     * Returns a random integer
     * @return random integer
     * */
    MKT_NODISCARD inline auto GetRandomUInt32(auto& seed = GetSeed()) -> UInt32_T {
        thread_local std::mt19937 mt{ seed() };

        return std::uniform_int_distribution<UInt32_T>{
                std::numeric_limits<UInt32_T>::min(),
                std::numeric_limits<UInt32_T>::max(),
        }(mt);
    }

    /**
     * Returns a random integer in the range given by the parameters.
     * Throws if the range is ill-formed.
     * @return random integer in the range [lowerBound, upperbound)
     * @throws std::runtime_error if the range is invalid
     * */
    MKT_NODISCARD inline auto GetRandomInt(Int32_T lowerBound, Int32_T upperbound, auto& seed = GetSeed()) -> Int32_T {
        if (lowerBound > upperbound)
            throw std::runtime_error(fmt::format("Invalid range for random integer generation. Lower bound is {}, upperbound is {}", lowerBound, upperbound));

        thread_local std::mt19937 mt{ seed() };
        return std::uniform_int_distribution{ lowerBound, upperbound }(mt);
    }

    /**
     * Returns a random real number in the range given by the parameters.
     * Throws if the range is ill-formed
     * @return random integer in the range [lowerBound, upperbound)
     * @throws std::runtime_error if the range is invalid
     * */
    MKT_NODISCARD inline auto GetRandomReal(double lowerBound, double upperbound, auto& seed = GetSeed()) -> double {
        if (lowerBound > upperbound)
            throw std::runtime_error(fmt::format("Invalid range for random integer generation. Lower bound is {}, upperbound is {}", lowerBound, upperbound));

        thread_local std::mt19937 mt{ seed() };
        return std::uniform_real_distribution{ lowerBound, upperbound }(mt);
    }

    MKT_NODISCARD inline auto GetRandomIntNumberList(Size_T length, Int32_T lowerBound = -1000, Int32_T upperbound = 1000, auto& seed = GetSeed()) -> std::vector<Int32_T> {
        std::vector<Int32_T> result{};

        Size_T count{};
        result.reserve(length);
        for ( ; count < length; ++count)
            result.emplace_back(GetRandomInt(lowerBound, upperbound, seed));

        return result;
    }

    MKT_NODISCARD inline auto GetRandomRealNumberList(Size_T length, double lowerBound = -1000.0, double upperbound = 1000.0, auto& seed = GetSeed()) -> std::vector<double> {
        std::vector<double> result{};

        Size_T count{};
        result.reserve(length);
        for ( ; count < length; ++count)
            result.emplace_back(GetRandomReal(lowerBound, upperbound, seed));

        return result;
    }
}

// TODO: think about prebatching bunch of random guids in some sort of pool and give them out as they are needed, create more when the pool gets empty
namespace Mikoto::Random::GUID {
    inline auto GetGUIDs() -> std::unordered_set<UInt64_T>& {
        static std::unordered_set<UInt64_T> guids{};

        return guids;
    }

    /**
     * Inserts guid to the list of valid guids
     * @param guid new guid
     * */
    inline auto ValidateGUID(UInt64_T guid) -> void {
        GetGUIDs().emplace(guid);
    }

    /**
     * Removes guid from the list of valid guids
     * @param guid id to be removed
     * */
    inline auto InvalidateGUID(UInt64_T guid) -> void {
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
    MKT_NODISCARD inline auto IsUse(UInt64_T guid) -> bool {
        return GetGUIDs().contains(guid);
    }

    /**
     * Returns a unique 64-bit integer every time it is called. This
     * function is used for the purpose of GUID's
     * @returns universally unique integer
     * */
    MKT_NODISCARD inline auto GenerateGUID() -> UInt64_T {
        thread_local auto& seed{ GetGUIDSeed() };

        UInt64_T result{ 0 };

        do
            result = GetRandomUInt64(seed);
        while(IsUse(result));

        ValidateGUID(result);

        return result;
    }

    class UUID {
    public:
        explicit UUID()
            :   m_Id{}
        {
            m_Id = GenerateGUID();
        }

        /**
         * Initializes this GUID with the given id. id should've been generated via
         * GenerateGUID(), otherwise it has to be validated.
         * */
        explicit UUID(UInt64_T id)
            :   m_Id{ id }
        {
            if (IsUse(id)) {
                m_Id = GenerateGUID();
                MKT_CORE_LOGGER_WARN("Already consumed UUID {} being used again. GUID Regenerated {}", id, m_Id);
            }
        }

        explicit operator UInt64_T () const { return m_Id; }

        MKT_NODISCARD auto Get() const -> UInt64_T { return m_Id; }

        ~UUID() { InvalidateGUID(m_Id); }

    private:
        UInt64_T m_Id{};
    };
}
#endif // MIKOTO_RANDOM_HH
