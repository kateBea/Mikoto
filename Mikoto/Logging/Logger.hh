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

#ifndef MIKOTO_LOGGER_HH
#define MIKOTO_LOGGER_HH

#include <memory>

#include <EASTL/string.h>

#include <cpptrace/cpptrace.hpp>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Singleton.hh>

namespace mikoto::core::stacktrace {

    MKT_NODISCARD auto ToString() -> eastl::string;
}

namespace mikoto::core {

    enum class LoggingSeverity {
        eDebug,
        eInfo,
        eWarning,
        eError,
        eCritical,
    };

    class Logger final : public Singleton<Logger> {
    public:
        explicit Logger();

        auto GetStdOutLog() -> const std::shared_ptr<spdlog::logger>&;
        auto GetStdErrLog() -> const std::shared_ptr<spdlog::logger>&;
        auto GetStdFileLog() -> const std::shared_ptr<spdlog::logger>&;

    private:
        auto Init() -> void;

        // spdlog uses std::shared_ptr
        std::shared_ptr<spdlog::logger> mStdOut{};
        std::shared_ptr<spdlog::logger> mStdErr{};
        std::shared_ptr<spdlog::logger> mFile{};

        eastl::string mFileLogName{ "" };

        LoggingSeverity mLoggingSeverity{ LoggingSeverity::eDebug };
    };
}

#if !defined(NDEBUG)
    #define MKT_ENABLE_LOGGING
#else
    #undef MKT_ENABLE_LOGGING
#endif

#if defined(MKT_ENABLE_LOGGING)
    #define MKT_CORE_LOGGER_ERROR(...) mikoto::core::Logger::Get()->GetStdErrLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_WARN(...) mikoto::core::Logger::Get()->GetStdOutLog()->warn(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_CRITICAL(...) mikoto::core::Logger::Get()->GetStdErrLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_TRACE(...) mikoto::core::Logger::Get()->GetStdOutLog()->trace(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_INFO(...) mikoto::core::Logger::Get()->GetStdOutLog()->info(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_DEBUG(...) mikoto::core::Logger::Get()->GetStdOutLog()->debug(fmt::format(__VA_ARGS__))

    #define MKT_FILE_LOGGER_ERROR(...) mikoto::core::Logger::Get()->GetStdFileLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_WARN(...) mikoto::core::Logger::Get()->GetStdFileLog()->warn(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_CRITICAL(...) mikoto::core::Logger::Get()->GetStdFileLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_TRACE(...) mikoto::core::Logger::Get()->GetStdFileLog()->trace(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_INFO(...) mikoto::core::Logger::Get()->GetStdFileLog()->info(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_DEBUG(...) mikoto::core::Logger::Get()->GetStdFileLog()->debug(fmt::format(__VA_ARGS__))

    #define MKT_PRINT_STACK_TRACE() cpptrace::generate_trace().print()
#else
    // I still want to see errors and critical logs
    #define MKT_CORE_LOGGER_ERROR(...) mikoto::core::Logger::Get()->GetStdErrLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_CRITICAL(...) mikoto::core::Logger::Get()->GetStdErrLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_INFO(...) mikoto::core::Logger::Get()->GetStdOutLog()->info(fmt::format(__VA_ARGS__))

    #define MKT_CORE_LOGGER_TRACE(...)
    #define MKT_CORE_LOGGER_WARN(...)
    #define MKT_CORE_LOGGER_DEBUG(...)

    #define MKT_FILE_LOGGER_CRITICAL(...) mikoto::core::Logger::Get()->GetStdFileLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_ERROR(...) mikoto::core::Logger::Get()->GetStdFileLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_INFO(...) mikoto::core::Logger::Get()->GetStdFileLog()->info(fmt::format(__VA_ARGS__))

    #define MKT_FILE_LOGGER_WARN(...)
    #define MKT_FILE_LOGGER_TRACE(...)
    #define MKT_FILE_LOGGER_DEBUG(...)

    #define MKT_STACK_TRACE() cpptrace::generate_trace().print()
#endif


#endif // MIKOTO_LOGGER_HH
