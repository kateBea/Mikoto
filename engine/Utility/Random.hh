/**
 * Random.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_RANDOM_HH
#define MIKOTO_RANDOM_HH

// C++ Standard Library
#include <random>
#include <stdexcept>
#include <vector>

// Third-Party Library
#include <fmt/format.h>

// Project Headers
#include <Utility/Types.hh>
#include <Utility/Common.hh>

/**
 * This namespace offers some convenient functions to generate random real numbers and
 * integers. There's still some testing to be done on them.
 *
 * TODO: review (thread safety, etc)
 * */
namespace Mikoto::Random {
    /**
     * Returns a seed for random number generation
     * @returns seed
     * */
    MKT_NODISCARD inline auto GetSeed() -> decltype(auto) {
        static auto seed{ std::random_device{}() };
        return seed;
    }

    /**
     * Returns a random integer in the range given by the parameters.
     * Throws if the range is ill-formed.
     * @return random integer in the range [lowerBound, upperbound)
     * @throws std::runtime_error if the range is invalid
     * */
    MKT_NODISCARD inline auto GetRandomInt(Int32_T lowerBound, Int32_T upperbound) -> Int32_T {
        if (lowerBound > upperbound)
            throw std::runtime_error(fmt::format("Invalid range for random integer generation. Lower bound is {}, upperbound is {}", lowerBound, upperbound));

        static std::mt19937 mt{ GetSeed() };
        std::uniform_int_distribution dist{ lowerBound, upperbound };

        return dist(mt);
    }

    /**
     * Returns a random real number in the range given by the parameters.
     * Throws if the range is ill-formed
     * @return random integer in the range [lowerBound, upperbound)
     * @throws std::runtime_error if the range is invalid
     * */
    MKT_NODISCARD inline auto GetRandomReal(double lowerBound, double upperbound) -> double {
        if (lowerBound > upperbound)
            throw std::runtime_error(fmt::format("Invalid range for random integer generation. Lower bound is {}, upperbound is {}", lowerBound, upperbound));

        static std::mt19937 mt{ GetSeed() };
        std::uniform_real_distribution dist{ lowerBound, upperbound };

        return dist(mt);
    }

    MKT_NODISCARD inline auto GetRandomIntNumberList(Size_T length, Int32_T lowerBound = -1000, Int32_T upperbound = 1000) -> std::vector<Int32_T> {
        std::vector<Int32_T> result{};

        Size_T count{};
        result.reserve(length);
        for ( ; count < length; ++count)
            result.emplace_back(GetRandomInt(lowerBound, upperbound));

        return result;
    }

    MKT_NODISCARD inline auto GetRandomRealNumberList(Size_T length, double lowerBound = -1000.0, double upperbound = 1000.0) -> std::vector<double> {
        std::vector<double> result{};

        Size_T count{};
        result.reserve(length);
        for ( ; count < length; ++count)
            result.emplace_back(GetRandomReal(lowerBound, upperbound));

        return result;
    }
}
#endif // MIKOTO_RANDOM_HH
