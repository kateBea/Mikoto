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

#include <Core/Profiler.hh>

#include <Networking/HttpRequest.hh>

namespace mikoto::network {

    auto ParseHttpRequest( const eastl::string_view raw, HttpRequest& request ) -> bool {
        MKT_BEGIN_PROFILER();
        const core::usize headerEnd{ raw.find( "\r\n\r\n" ) };
        const core::usize lineEnd{ raw.find( "\r\n" ) };

        if ( headerEnd == eastl::string_view::npos || lineEnd == eastl::string_view::npos ) {
            return false;
        }

        const eastl::string_view startLine{ raw.substr( 0, lineEnd ) };
        const core::usize firstSpace{ startLine.find( ' ' ) };
        const core::usize secondSpace{ startLine.find( ' ', firstSpace + 1 ) };

        if ( firstSpace == eastl::string_view::npos || secondSpace == eastl::string_view::npos ) {
            return false;
        }

        request = {};
        request.mMethod = startLine.substr( 0, firstSpace );
        request.mPath = startLine.substr( firstSpace + 1, secondSpace - firstSpace - 1 );
        request.mVersion = startLine.substr( secondSpace + 1 );

        if ( !ParseHttpHeaders( raw.substr( lineEnd + 2, headerEnd - lineEnd + 2 ), request.mHeaders ) ) {
            return false;
        }

        request.mBody = raw.substr( headerEnd + 4 );
        return true;
    }

}// namespace mikoto::network
