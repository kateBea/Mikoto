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
#include <Common/Common.hh>
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    /**
     * Holds the CORE logger and the APP logger. The CORE logger
     * logs information about the current state of the Core systems of the engine.
     * The APP logger logs information about the current state of the Application,
     * which manages and serves as a central Hub for our engine
     * */
    class Logger final : public Singleton<Logger> {
    public:
        explicit Logger()
            : m_CoreLogger{ nullptr }, m_AppLogger{ nullptr }  { Init(); }

        auto GetCoreLogger() -> const Ref_T<spdlog::logger>&;
        auto GetAppLogger() -> const Ref_T<spdlog::logger>&;

    private:
        auto Init() -> void;

    private:
        Ref_T<spdlog::logger> m_CoreLogger{};
        Ref_T<spdlog::logger> m_AppLogger{};
    };
}

// Enable logging for debug builds exclusively
#if !defined(NDEBUG)
    #define MKT_ENABLE_LOGGING
#else
    #undef MKT_ENABLE_LOGGING
#endif

#if defined(MKT_ENABLE_LOGGING)
    #define MKT_CORE_LOGGER_ERROR(...) Mikoto::Logger::Get().GetCoreLogger()->error(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_WARN(...) Mikoto::Logger::Get().GetCoreLogger()->warn(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_CRITICAL(...) Mikoto::Logger::Get().GetCoreLogger()->critical(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_TRACE(...) Mikoto::Logger::Get().GetCoreLogger()->trace(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_INFO(...) Mikoto::Logger::Get().GetCoreLogger()->info(fmt::format(__VA_ARGS__))
    #define MKT_CORE_LOGGER_DEBUG(...) Mikoto::Logger::Get().GetCoreLogger()->debug(fmt::format(__VA_ARGS__))

    #define MKT_APP_LOGGER_ERROR(...) Mikoto::Logger::Get().GetAppLogger()->error(fmt::format(__VA_ARGS__))
    #define MKT_APP_LOGGER_WARN(...) Mikoto::Logger::Get().GetAppLogger()->warn(fmt::format(__VA_ARGS__))
    #define MKT_APP_LOGGER_CRITICAL(...) Mikoto::Logger::Get().GetAppLogger()->critical(fmt::format(__VA_ARGS__))
    #define MKT_APP_LOGGER_TRACE(...) Mikoto::Logger::Get().GetAppLogger()->trace(fmt::format(__VA_ARGS__))
    #define MKT_APP_LOGGER_INFO(...) Mikoto::Logger::Get().GetAppLogger()->info(fmt::format(__VA_ARGS__))
    #define MKT_APP_LOGGER_DEBUG(...) Mikoto::Logger::Get().GetAppLogger()->debug(fmt::format(__VA_ARGS__))
#else
    #define MKT_CORE_LOGGER_ERROR(...)
    #define MKT_CORE_LOGGER_WARN(...)
    #define MKT_CORE_LOGGER_CRITICAL(...)
    #define MKT_CORE_LOGGER_TRACE(...)
    #define MKT_CORE_LOGGER_INFO(...)
    #define MKT_CORE_LOGGER_DEBUG(...)

    #define MKT_APP_LOGGER_ERROR(...)
    #define MKT_APP_LOGGER_WARN(...)
    #define MKT_APP_LOGGER_CRITICAL(...)
    #define MKT_APP_LOGGER_TRACE(...)
    #define MKT_APP_LOGGER_INFO(...)
    #define MKT_APP_LOGGER_DEBUG(...)
#endif


#endif // MIKOTO_LOGGER_HH
