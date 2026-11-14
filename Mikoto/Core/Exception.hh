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

#ifndef MIKOTO_CORE_EXCEPTION_HH
#define MIKOTO_CORE_EXCEPTION_HH

#include <exception>

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <cpptrace/cpptrace.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

namespace mikoto::core {
    // Error codes
    enum class ErrorCode {
        eInitialization_Failed,
    };

    struct Error {
        ErrorCode mErrorCode{};
        eastl::string mMessage{};

    };

    class Exception : public std::exception {
    public:

        // Named to GetReason because windows is annoying with macros
        MKT_NODISCARD auto GetReason() const noexcept -> const eastl::string&;

    private:
        eastl::string mMessage{};
    };

    class RuntimeException : public std::exception {
    public:
        explicit RuntimeException( eastl::string msg );

        explicit RuntimeException( const char* msg );

        RuntimeException( const RuntimeException& ) = default;
        RuntimeException& operator=( const RuntimeException& ) = default;

        RuntimeException( RuntimeException&& ) noexcept = default;
        RuntimeException& operator=( RuntimeException&& ) noexcept = default;

        ~RuntimeException() override = default;

        MKT_NODISCARD auto what() const noexcept -> const char* override;
        MKT_NODISCARD auto Message() const noexcept -> const eastl::string&;

    private:
        eastl::string mMessage{};
    };

    class LogicError : public RuntimeException {
    public:
        using RuntimeException::RuntimeException;
    };

    class InvalidArgument : public LogicError {
    public:
        using LogicError::LogicError;
    };

    class DomainError : public LogicError {
    public:
        using LogicError::LogicError;
    };

    class LengthError : public LogicError {
    public:
        using LogicError::LogicError;
    };

    class OutOfRange : public LogicError {
    public:
        using LogicError::LogicError;
    };

    class FutureError : public LogicError {
    public:
        using LogicError::LogicError;
    };

    class RangeError : public RuntimeException {
    public:
        using RuntimeException::RuntimeException;
    };

#define MKT_THROW_EXCEPTION( EXCEPTION_TYPE, MESSAGE )       \
    do {                                                     \
        cpptrace::generate_trace().print();                  \
        throw mikoto::core::EXCEPTION_TYPE(                  \
                mikoto::string::Format(                      \
                        "Message: {}\n@File: {}\n@Line: {}", \
                        MESSAGE,                             \
                        __FILE__,                            \
                        __LINE__ ) );                        \
    } while ( false )

#define MKT_THROW_RUNTIME_ERROR(MESSAGE) \
    MKT_THROW_EXCEPTION( RuntimeException, MESSAGE )

#define MKT_THROW_LOGIC_ERROR(MESSAGE) \
    MKT_THROW_EXCEPTION( LogicError, MESSAGE )

#define MKT_THROW_INVALID_ARGUMENT(MESSAGE) \
    MKT_THROW_EXCEPTION( InvalidArgument, MESSAGE )

#define MKT_THROW_DOMAIN_ERROR(MESSAGE) \
    MKT_THROW_EXCEPTION( DomainError, MESSAGE )

#define MKT_THROW_LENGTH_ERROR(MESSAGE) \
    MKT_THROW_EXCEPTION( LengthError, MESSAGE )

#define MKT_THROW_OUT_OF_RANGE(MESSAGE) \
    MKT_THROW_EXCEPTION( OutOfRange, MESSAGE )

#define MKT_THROW_FUTURE_ERROR(MESSAGE) \
    MKT_THROW_EXCEPTION( FutureError, MESSAGE )

#define MKT_THROW_RANGE_ERROR(MESSAGE) \
    MKT_THROW_EXCEPTION( RangeError, MESSAGE )

}
#endif// MIKOTO_CORE_EXCEPTION_HH
