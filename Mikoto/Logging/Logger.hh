/**
 * Logger.hh
 * Created by kate on 5/25/23.
 * */

#ifndef MIKOTO_LOGGER_HH
#define MIKOTO_LOGGER_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <spdlog/spdlog.h>

// Spdlog already has fmt bundled, but we are using it
// as an external library
#include <fmt/format.h>

// Project Headers
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    enum class LoggingSeverity {
        LOGGING_SEVERITY_DEBUG = 0,
        LOGGING_SEVERITY_INFO,
        LOGGING_SEVERITY_WARNING,
        LOGGING_SEVERITY_ERROR,
        LOGGING_SEVERITY_CRITICAL,
    };

    class Logger final : public Singleton<Logger> {
    public:
        explicit Logger();

        auto GetStdOutLog() -> const Shared<spdlog::logger>&;
        auto GetStdErrLog() -> const Shared<spdlog::logger>&;
        auto GetStdFileLog() -> const Shared<spdlog::logger>&;

    private:
        auto Init() -> void;

        Shared<spdlog::logger> m_StdOut{};
        Shared<spdlog::logger> m_StdErr{};
        Shared<spdlog::logger> m_File{};

        std::string m_FileLogName{ "" };

        LoggingSeverity m_LoggingSeverity{ LoggingSeverity::LOGGING_SEVERITY_DEBUG };
    };
}

#if !defined(NDEBUG)
    #define MKT_ENABLE_LOGGING
#else
    #undef MKT_ENABLE_LOGGING
#endif

#if defined(MKT_ENABLE_LOGGING)
    #define MKT_CORE_LOGGER_ERROR(...) Mikoto::Logger::Get()->GetStdErrLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_WARN(...) Mikoto::Logger::Get()->GetStdOutLog()->warn(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_CRITICAL(...) Mikoto::Logger::Get()->GetStdErrLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_TRACE(...) Mikoto::Logger::Get()->GetStdOutLog()->trace(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_INFO(...) Mikoto::Logger::Get()->GetStdOutLog()->info(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_DEBUG(...) Mikoto::Logger::Get()->GetStdOutLog()->debug(fmt::format(__VA_ARGS__))

    #define MKT_FILE_LOGGER_ERROR(...) Mikoto::Logger::Get()->GetStdFileLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_WARN(...) Mikoto::Logger::Get()->GetStdFileLog()->warn(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_CRITICAL(...) Mikoto::Logger::Get()->GetStdFileLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_TRACE(...) Mikoto::Logger::Get()->GetStdFileLog()->trace(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_INFO(...) Mikoto::Logger::Get()->GetStdFileLog()->info(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_DEBUG(...) Mikoto::Logger::Get()->GetStdFileLog()->debug(fmt::format(__VA_ARGS__))

    #define MKT_STACK_TRACE() cpptrace::generate_trace().print()
#else
    // I still want to see errors and critical logs
    #define MKT_CORE_LOGGER_ERROR(...) Mikoto::Logger::Get()->GetStdErrLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_CRITICAL(...) Mikoto::Logger::Get()->GetStdErrLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_INFO(...) Mikoto::Logger::Get()->GetStdOutLog()->info(fmt::format(__VA_ARGS__))

    #define MKT_CORE_LOGGER_TRACE(...)
    #define MKT_CORE_LOGGER_WARN(...)
    #define MKT_CORE_LOGGER_DEBUG(...)

    #define MKT_FILE_LOGGER_CRITICAL(...) Mikoto::Logger::Get()->GetStdFileLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_ERROR(...) Mikoto::Logger::Get()->GetStdFileLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_FILE_LOGGER_INFO(...) Mikoto::Logger::Get()->GetStdFileLog()->info(fmt::format(__VA_ARGS__))

    #define MKT_FILE_LOGGER_WARN(...)
    #define MKT_FILE_LOGGER_TRACE(...)
    #define MKT_FILE_LOGGER_DEBUG(...)

    #define MKT_STACK_TRACE() cpptrace::generate_trace().print()
#endif


#endif // MIKOTO_LOGGER_HH
