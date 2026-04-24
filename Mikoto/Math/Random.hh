//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_RANDOM_HH
#define MIKOTO_RANDOM_HH

#include <random>
#include <limits>
#include <vector>
#include <stdexcept>
#include <unordered_set>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Exception.hh>

namespace mikoto::math::random {

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
    MKT_NODISCARD inline auto GetRandomUInt64(std::random_device& seed = GetSeed()) -> u64 {
        thread_local std::mt19937_64 mt{ seed() };

        return std::uniform_int_distribution<u64>{
                (std::numeric_limits<u64>::min)(),
                (std::numeric_limits<u64>::max)(),
        }(mt);
    }


    /**
     * Returns a random integer
     * @return random integer
     * */
    MKT_NODISCARD inline auto GetRandomUInt32(std::random_device& seed = GetSeed()) -> u32 {
        thread_local std::mt19937 mt{ seed() };

        return std::uniform_int_distribution<u32>{
                (std::numeric_limits<u32>::min)(),
            (std::numeric_limits<u32>::max)(),
        }(mt);
    }


    /**
     * Returns a random integer in the range given by the parameters.
     * Throws if the range is ill-formed.
     * @return random integer in the range [lowerBound, upperbound)
     * @throws std::runtime_error if the range is invalid
     * */
    MKT_NODISCARD inline auto GetRandomInt(i32 lowerBound, i32 upperbound, std::random_device& seed = GetSeed()) -> i32 {
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


    MKT_NODISCARD inline auto GetRandomIntNumberList(size_t length, i32 lowerBound = -1000, i32 upperbound = 1000, std::random_device& seed = GetSeed()) -> std::vector<i32> {
        std::vector<i32> result{};

        size_t count{};
        result.reserve(length);
        for ( ; count < length; ++count)
            result.emplace_back(GetRandomInt(lowerBound, upperbound, seed));

        return result;
    }


    MKT_NODISCARD inline auto GetRandomRealNumberList(size_t length, double lowerBound = -1000.0, double upperbound = 1000.0, std::random_device& seed = GetSeed()) -> std::vector<double> {
        std::vector<double> result{};

        size_t count{};
        result.reserve(length);
        for ( ; count < length; ++count)
            result.emplace_back(GetRandomReal(lowerBound, upperbound, seed));

        return result;
    }

    inline auto GetGUIDs() -> std::unordered_set<u64>& {
        static std::unordered_set<u64> guids{};

        return guids;
    }

    /**
     * Inserts guid to the list of valid guids
     * @param guid new guid
     * */
    inline auto ValidateGUID(u64 guid) -> void {
        GetGUIDs().emplace(guid);
    }

    /**
     * Removes guid from the list of valid guids
     * @param guid id to be removed
     * */
    inline auto InvalidateGUID(u64 guid) -> void {
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
    MKT_NODISCARD inline auto IsUse(u64 guid) -> bool {
        return GetGUIDs().contains(guid);
    }

    /**
     * Returns a unique 64-bit integer every time it is called. This
     * function is used for the purpose of GUID's
     * @returns universally unique integer
     * */
    MKT_NODISCARD inline auto GenerateGUID() -> u64 {
        static auto& seed{ GetGUIDSeed() };

        u64 result{ 0 };

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

        explicit operator u64 () const { return m_Id; }

        MKT_NODISCARD auto Get() const -> u64 { return m_Id; }

        ~GlobalUniqueID() { InvalidateGUID(m_Id); }

    private:
        u64 m_Id{};
    };
}
#endif // MIKOTO_RANDOM_HH
