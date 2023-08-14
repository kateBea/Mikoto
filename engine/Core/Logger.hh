/**
 * Logger.hh
 * Created by kate on 5/25/23.
 * */

#ifndef KATE_ENGINE_LOGGER_HH
#define KATE_ENGINE_LOGGER_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// Spdlog already has fmt bundled, but we are using it
// as an external library
#include <fmt/format.h>

// Project Headers
#include <Utility/Singleton.hh>
#include <Utility/Common.hh>

namespace Mikoto {
    /**
     * Holds the CORE logger and the APP logger. The CORE logger
     * logs information about the current state of the Core systems of the engine.
     * The APP logger logs information about the current state of the Application,
     * which manages and serves as a central Hub for our engine
     * */
    class Logger : public Singleton<Logger> {
    public:
        Logger() : m_CoreLogger{ nullptr }, m_AppLogger{ nullptr }  { Init(); }

        auto GetCoreLogger() -> const std::shared_ptr<spdlog::logger>&;
        auto GetAppLogger() -> const std::shared_ptr<spdlog::logger>&;

    private:
        auto Init() -> void;

        std::shared_ptr<spdlog::logger> m_CoreLogger{};
        std::shared_ptr<spdlog::logger> m_AppLogger{};
    };
}

#if defined(NDEBUG) || defined(_DEBUG)
    #define KT_ENABLE_LOGGING
#else
    #undef KT_ENABLE_LOGGING
#endif

// Log macros
#if defined(KT_ENABLE_LOGGING)

    #define KATE_CORE_LOGGER_ERROR(...) Mikoto::Logger::Get().GetCoreLogger()->error(fmt::format(__VA_ARGS__))
    #define KATE_CORE_LOGGER_WARN(...) Mikoto::Logger::Get().GetCoreLogger()->warn(fmt::format(__VA_ARGS__))
    #define KATE_CORE_LOGGER_CRITICAL(...) Mikoto::Logger::Get().GetCoreLogger()->critical(fmt::format(__VA_ARGS__))
    #define KATE_CORE_LOGGER_TRACE(...) Mikoto::Logger::Get().GetCoreLogger()->trace(fmt::format(__VA_ARGS__))
    #define KATE_CORE_LOGGER_INFO(...) Mikoto::Logger::Get().GetCoreLogger()->info(fmt::format(__VA_ARGS__))
    #define KATE_CORE_LOGGER_DEBUG(...) Mikoto::Logger::Get().GetCoreLogger()->debug(fmt::format(__VA_ARGS__))

    #define KATE_APP_LOGGER_ERROR(...) Mikoto::Logger::Get().GetAppLogger()->error(fmt::format(__VA_ARGS__))
    #define KATE_APP_LOGGER_WARN(...) kaTe::Logger::Get().GetAppLogger()->warn(fmt::format(__VA_ARGS__))
    #define KATE_APP_LOGGER_CRITICAL(...) Mikoto::Logger::Get().GetAppLogger()->critical(fmt::format(__VA_ARGS__))
    #define KATE_APP_LOGGER_TRACE(...) kaTe::Logger::Get().GetAppLogger()->trace(fmt::format(__VA_ARGS__))
    #define KATE_APP_LOGGER_INFO(...) Mikoto::Logger::Get().GetAppLogger()->info(fmt::format(__VA_ARGS__))
    #define KATE_APP_LOGGER_DEBUG(...) kaTe::Logger::Get().GetAppLogger()->debug(fmt::format(__VA_ARGS__))
#else
    #define KATE_CORE_LOGGER_ERROR(...)
    #define KATE_CORE_LOGGER_WARN(...)
    #define KATE_CORE_LOGGER_CRITICAL(...)
    #define KATE_CORE_LOGGER_TRACE(...)
    #define KATE_CORE_LOGGER_INFO(...)
    #define KATE_CORE_LOGGER_DEBUG(...)

    #define KATE_APP_LOGGER_ERROR(...)
    #define KATE_APP_LOGGER_WARN(...)
    #define KATE_APP_LOGGER_CRITICAL(...)
    #define KATE_APP_LOGGER_TRACE(...)
    #define KATE_APP_LOGGER_INFO(...)
    #define KATE_APP_LOGGER_DEBUG(...)
#endif


#endif // LOGGER_H
