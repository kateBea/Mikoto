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

        auto GetConsoleLog() -> const Shared<spdlog::logger>&;

    private:
        auto Init() -> void;

        Shared<spdlog::logger> m_CoreLogger{};
        LoggingSeverity m_LoggingSeverity{ LoggingSeverity::LOGGING_SEVERITY_DEBUG };
    };
}

#if !defined(NDEBUG)
    #define MKT_ENABLE_LOGGING
#else
    #undef MKT_ENABLE_LOGGING
#endif

#if defined(MKT_ENABLE_LOGGING)
    #define MKT_CORE_LOGGER_ERROR(...) Mikoto::Logger::Get().GetConsoleLog()->error(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_WARN(...) Mikoto::Logger::Get().GetConsoleLog()->warn(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_CRITICAL(...) Mikoto::Logger::Get().GetConsoleLog()->critical(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_TRACE(...) Mikoto::Logger::Get().GetConsoleLog()->trace(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_INFO(...) Mikoto::Logger::Get().GetConsoleLog()->info(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_DEBUG(...) Mikoto::Logger::Get().GetConsoleLog()->debug(fmt::format(__VA_ARGS__))
#else
    #define MKT_CORE_LOGGER_ERROR(...)
    #define MKT_CORE_LOGGER_WARN(...)
    #define MKT_CORE_LOGGER_CRITICAL(...)
    #define MKT_CORE_LOGGER_TRACE(...)
    #define MKT_CORE_LOGGER_INFO(...)
    #define MKT_CORE_LOGGER_DEBUG(...)
#endif


#endif // MIKOTO_LOGGER_HH
