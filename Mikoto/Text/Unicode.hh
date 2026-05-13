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

#ifndef MIKOTO_UNICODE_HH
#define MIKOTO_UNICODE_HH

#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

namespace mikoto::text {

    using UnicodePoint = core::u32;

    // Use a better Unicode string container
    MKT_NODISCARD auto ToUnicode( eastl::string_view utf8 ) -> eastl::vector<UnicodePoint>;

    // Move to Text/Unicode
    MKT_NODISCARD inline auto DecodeUTF8( const eastl::string& ut8 ) -> eastl::vector<core::u32> {
        eastl::vector<core::u32> codepoints{};
        for ( core::size_t i{}; i < ut8.size(); ) {
            core::u32 cp{};

            unsigned char c{ static_cast<unsigned char>( ut8[i] ) };

            if ( c < 0x80 ) {
                cp = c;
                i += 1;
            } else if ( ( c >> 5 ) == 0x6 ) {
                cp = ( ( c & 0x1F ) << 6 ) |
                     ( static_cast<unsigned char>( ut8[i + 1] ) & 0x3F );
                i += 2;
            } else if ( ( c >> 4 ) == 0xE ) {
                cp = ( ( c & 0x0F ) << 12 ) |
                     ( ( static_cast<unsigned char>( ut8[i + 1] ) & 0x3F ) << 6 ) |
                     ( static_cast<unsigned char>( ut8[i + 2] ) & 0x3F );
                i += 3;
            } else if ( ( c >> 3 ) == 0x1E ) {
                cp = ( ( c & 0x07 ) << 18 ) |
                     ( ( static_cast<unsigned char>( ut8[i + 1] ) & 0x3F ) << 12 ) |
                     ( ( static_cast<unsigned char>( ut8[i + 2] ) & 0x3F ) << 6 ) |
                     ( static_cast<unsigned char>( ut8[i + 3] ) & 0x3F );
                i += 4;
            }

            codepoints.push_back( cp );
        }

        return codepoints;
    }

}// namespace Mikoto

#endif//MIKOTO_UNICODE_HH
