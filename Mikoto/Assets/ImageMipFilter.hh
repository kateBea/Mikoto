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

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace mikoto::asset {

    // An exact area box filter keeps odd-sized edges in the mip instead of
    // dropping the final texel. Channels retain their existing numeric encoding.
    inline auto FilterImageMipLevel( const void *source, uint32_t width, uint32_t height,
        void *destination, uint32_t nextWidth, uint32_t nextHeight, bool floating ) -> void {
        const auto *src{ static_cast<const uint8_t *>( source ) };
        auto *dst{ static_cast<uint8_t *>( destination ) };
        const size_t pixelBytes{ 4 * ( floating ? sizeof( float ) : 1 ) };
        for ( uint32_t y{}; y < nextHeight; ++y ) {
            const double top{ double( y ) * height / nextHeight }, bottom{ double( y + 1 ) * height / nextHeight };
            for ( uint32_t x{}; x < nextWidth; ++x ) {
                const double left{ double( x ) * width / nextWidth }, right{ double( x + 1 ) * width / nextWidth };
                double sum[4]{};
                for ( uint32_t sy{ uint32_t( top ) }; sy < uint32_t( std::ceil( bottom ) ); ++sy ) {
                    for ( uint32_t sx{ uint32_t( left ) }; sx < uint32_t( std::ceil( right ) ); ++sx ) {
                        const double weight{ ( std::min( right, double( sx + 1 ) ) - std::max( left, double( sx ) ) ) *
                                             ( std::min( bottom, double( sy + 1 ) ) - std::max( top, double( sy ) ) ) };
                        const auto index{ ( size_t( sy ) * width + sx ) * pixelBytes };
                        for ( uint32_t c{}; c < 4; ++c ) {
                            float value{};
                            if ( floating ) {
                                std::memcpy( &value, src + index + c * sizeof( float ), sizeof( float ) );
                            }
                            else {
                                value = src[index + c];
                            }
                            sum[c] += value * weight;
                        }
                    }
                }
                const auto output{ ( size_t( y ) * nextWidth + x ) * pixelBytes };
                for ( uint32_t c{}; c < 4; ++c ) {
                    const float value{ float( sum[c] / ( ( right - left ) * ( bottom - top ) ) ) };
                    if ( floating ) {
                        std::memcpy( dst + output + c * sizeof( float ), &value, sizeof( float ) );
                    }
                    else {
                        dst[output + c] = uint8_t( std::clamp( std::round( value ), 0.0f, 255.0f ) );
                    }
                }
            }
        }
    }
}// namespace mikoto::asset
