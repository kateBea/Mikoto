//    Copyright 2025 ケイト
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

#ifndef MIKOTO_MATH_HH
#define MIKOTO_MATH_HH

// I love Windows.h defining min and max macros that break everything
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <numbers>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto::Math {

    MKT_NODISCARD auto Floor(double value) -> double;

    MKT_NODISCARD auto Log2(double value) -> double;

    MKT_NODISCARD auto ToRadians(double value) -> double;
    MKT_NODISCARD auto ToDegrees(double value) -> double;


    MKT_NODISCARD auto Abs(double value) -> double;

    MKT_NODISCARD auto Lerp(float a, float b, float f) -> double;

    auto Recompose( Mat4F& transform, const Vec3F& translation, const Vec3F& rotation, const Vec3F& scale ) -> void;
    auto Decompose( const Mat4F& transform, Vec3F& translation, Vec3F& rotation, Vec3F& scale ) -> void;

}

namespace Mikoto::Math::Constants {

    // Math Constants
    inline constexpr auto PI{ std::numbers::pi_v<double> };

}


#endif //MIKOTO_MATH_HH