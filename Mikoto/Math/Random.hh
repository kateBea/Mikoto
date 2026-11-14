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

#ifndef MIKOTO_MATH_RANDOM_HH
#define MIKOTO_MATH_RANDOM_HH

#include <limits>
#include <random>
#include <vector>
#include <stdexcept>
#include <unordered_set>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

namespace mikoto::math::random {

    class Guid {
    public:
        explicit Guid();

        MKT_NODISCARD operator core::u32 () const;

        // TODO: Better Guid System
        auto Recreate() -> void;

        ~Guid() = default;

    private:
        core::u32 mId{};
    };

    class UniqueID {
    };

}// namespace mikoto

#endif// MIKOTO_MATH_RANDOM_HH
