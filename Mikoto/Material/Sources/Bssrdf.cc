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

// PBRT-derived portions: Copyright (c) 1998-2020 Matt Pharr, Wenzel Jakob,
// and Greg Humphreys. SPDX-License-Identifier: Apache-2.0

#include <cmath>

#include <Material/Bssrdf.hh>

namespace mikoto::material {
    using namespace mikoto::core;

    // Photon beam diffusion and Fresnel moments from PBRT v4.
    // https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/bssrdf.cpp
    // https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/util/scattering.cpp
    // https://pbr-book.org/3ed-2018/Light_Transport_II_Volume_Rendering/Subsurface_Scattering_Using_the_Diffusion_Equation
    static constexpr f32 kPi{ 3.14159265358979323846f };

    static auto FresnelMoment1( f32 eta ) -> f32 {
        const f32 e2{ eta * eta }, e3{ e2 * eta }, e4{ e3 * eta }, e5{ e4 * eta };
        if ( eta < 1 ) {
            return .45966f - 1.73965f * eta + 3.37668f * e2 - 3.904945f * e3 + 2.49277f * e4 - .68441f * e5;
        }
        return -4.61686f + 11.1136f * eta - 10.4646f * e2 + 5.11455f * e3 - 1.27198f * e4 + .12746f * e5;
    }

    static auto FresnelMoment2( f32 eta ) -> f32 {
        const f32 e2{ eta * eta }, e3{ e2 * eta }, e4{ e3 * eta }, e5{ e4 * eta };
        if ( eta < 1 ) {
            return .27614f - .87350f * eta + 1.12077f * e2 - .65095f * e3 + .07883f * e4 + .04860f * e5;
        }
        const f32 inverse{ 1 / eta };
        return -547.033f + 45.3087f * inverse * inverse * inverse - 218.725f * inverse * inverse +
               458.843f * inverse + 404.557f * eta - 189.519f * e2 + 54.9327f * e3 - 9.00603f * e4 + .63942f * e5;
    }

    static auto FresnelDielectric( f32 cosine, f32 eta ) -> f32 {
        if ( cosine < 0 ) {
            cosine = -cosine;
            eta = 1 / eta;
        }
        const f32 sine2{ ( 1 - cosine * cosine ) / ( eta * eta ) };
        if ( sine2 >= 1 ) {
            return 1;
        }
        const f32 transmitted{ std::sqrt( 1 - sine2 ) };
        const f32 parallel{ ( eta * cosine - transmitted ) / ( eta * cosine + transmitted ) };
        const f32 perpendicular{ ( cosine - eta * transmitted ) / ( cosine + eta * transmitted ) };
        return .5f * ( parallel * parallel + perpendicular * perpendicular );
    }

    static auto BeamDiffusionMS( f32 scattering, f32 absorption, f32 g, f32 eta, f32 radius ) -> f32 {
        const f32 reducedS{ scattering * ( 1 - g ) }, reducedT{ absorption + reducedS };
        const f32 albedo{ reducedS / reducedT };
        const f32 diffusion{ ( 2 * absorption + reducedS ) / ( 3 * reducedT * reducedT ) };
        const f32 transport{ std::sqrt( absorption / diffusion ) };
        const f32 moment1{ FresnelMoment1( eta ) }, moment2{ FresnelMoment2( eta ) };
        const f32 extrapolation{ -2 * diffusion * ( 1 + 3 * moment2 ) / ( 1 - 2 * moment1 ) };
        const f32 phiScale{ .25f * ( 1 - 2 * moment1 ) }, eScale{ .5f * ( 1 - 3 * moment2 ) };
        f32 result{};

        for ( u32 i{}; i < 100; ++i ) {
            const f32 realZ{ -std::log1p( -( i + .5f ) / 100 ) / reducedT };
            const f32 virtualZ{ -realZ + 2 * extrapolation };
            const f32 realD{ std::hypot( radius, realZ ) }, virtualD{ std::hypot( radius, virtualZ ) };
            const f32 realExp{ std::exp( -transport * realD ) }, virtualExp{ std::exp( -transport * virtualD ) };
            const f32 phi{ ( realExp / realD - virtualExp / virtualD ) / ( 4 * kPi * diffusion ) };
            const f32 irradiance{ ( realZ * ( 1 + transport * realD ) * realExp / ( realD * realD * realD ) -
                                     virtualZ * ( 1 + transport * virtualD ) * virtualExp / ( virtualD * virtualD * virtualD ) ) / ( 4 * kPi ) };
            const f32 correction{ 1 - std::exp( -2 * reducedT * ( realD + realZ ) ) };
            result += correction * albedo * albedo * ( phi * phiScale + irradiance * eScale );
        }
        return result / 100;
    }

    static auto BeamDiffusionSS( f32 scattering, f32 absorption, f32 g, f32 eta, f32 radius ) -> f32 {
        const f32 extinction{ absorption + scattering }, albedo{ scattering / extinction };
        const f32 critical{ radius * std::sqrt( eta * eta - 1 ) };
        f32 result{};

        for ( u32 i{}; i < 100; ++i ) {
            const f32 depth{ critical - std::log1p( -( i + .5f ) / 100 ) / extinction };
            const f32 distance{ std::hypot( radius, depth ) }, cosine{ depth / distance };
            const f32 denominator{ 1 + g * g + 2 * g * cosine };
            const f32 phase{ ( 1 - g * g ) / ( 4 * kPi * denominator * std::sqrt( denominator ) ) };
            result += albedo * std::exp( -extinction * ( distance + critical ) ) * phase *
                      ( 1 - FresnelDielectric( -cosine, eta ) ) * cosine / ( distance * distance );
        }
        return result / 100;
    }

    auto BuildBssrdfProfile( const SubsurfaceSettings& settings ) -> BssrdfProfile {
        BssrdfProfile result{};
        f32 radius{};

        for ( u32 i{}; i < kBssrdfRadiusSamples; ++i ) {
            radius = i == 0 ? 0 : i == 1 ? .0025f : radius * 1.2f;
            auto& profile{ result.mProfile[i] };
            profile.x = radius;

            for ( u32 c{}; c < 3; ++c ) {
                const f32 extinction{ settings.mAbsorption[c] + settings.mScattering[c] };
                const f32 rho{ extinction > 0 ? settings.mScattering[c] / extinction : 0 };
                // Zero absorption is evaluated with a tiny positive extinction fraction to
                // keep the diffusion coefficient finite, as the table approaches rho = 1.
                const f32 s{ glm::min( rho, 1.f - 1e-6f ) }, a{ 1 - s };
                const f32 value{ BeamDiffusionSS( s, a, settings.mAnisotropy, settings.mIndexOfRefraction, radius ) +
                                 BeamDiffusionMS( s, a, settings.mAnisotropy, settings.mIndexOfRefraction, radius ) };
                profile[c + 1] = glm::max( 0.f, 2 * kPi * radius * value );

                if ( i > 0 ) {
                    result.mCDF[i][c + 1] = result.mCDF[i - 1][c + 1] +
                        .5f * ( radius - result.mProfile[i - 1].x ) * ( profile[c + 1] + result.mProfile[i - 1][c + 1] );
                }
            }
        }
        return result;
    }
}
