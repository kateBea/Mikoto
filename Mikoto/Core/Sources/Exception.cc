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

#include <Core/Exception.hh>

namespace mikoto::core {
    auto Exception::GetReason() const noexcept -> const eastl::string& {
        return mMessage;
    }

    RuntimeException::RuntimeException( eastl::string msg )
        : mMessage( eastl::move( msg ) ) {
    }


    RuntimeException::RuntimeException( const char* msg )
        : mMessage( msg ? msg : "" ) {
    }


    auto RuntimeException::what() const noexcept -> const char* {
        MKT_BEGIN_PROFILER();
        return mMessage.c_str();
    }


    auto RuntimeException::Message() const noexcept -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mMessage;
    }
}
