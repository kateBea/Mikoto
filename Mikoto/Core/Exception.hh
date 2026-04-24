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

#ifndef MIKOTO_RUNTIME_EXCEPTION_HH
#define MIKOTO_RUNTIME_EXCEPTION_HH

#include <exception>

#include <EASTL/string.h>
#include <EASTL/utility.h>

#include <cpptrace/cpptrace.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

namespace mikoto::core {

    class RuntimeException : public std::exception {
    public:
        explicit RuntimeException( eastl::string msg )
            : mMessage( eastl::move( msg ) ) {}

        explicit RuntimeException( const char* msg )
            : mMessage( msg ? msg : "" ) {}

        // Default copy and move support
        RuntimeException( const RuntimeException& ) = default;
        RuntimeException& operator=( const RuntimeException& ) = default;
        RuntimeException( RuntimeException&& ) noexcept = default;
        RuntimeException& operator=( RuntimeException&& ) noexcept = default;

        MKT_NODISCARD auto what() const noexcept -> const char* override {
            return mMessage.c_str();
        }

        MKT_NODISCARD auto Message() const noexcept -> const eastl::string& {
            return mMessage;
        }

    private:
        i32 mLineNumber{};
        eastl::string mLine{};
        eastl::string mMessage{};
    };

#define MKT_THROW_RUNTIME_ERROR(MESSAGE) \
    cpptrace::generate_trace().print(); \
    throw core::RuntimeException( string::Format( "Message: {}\n@File: {}\n@Line: {}", MESSAGE, __FILE__, __LINE__ ) )
}

#endif // MIKOTO_RUNTIME_EXCEPTION_HH
