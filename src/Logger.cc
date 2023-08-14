#include <memory>

#include <Core/Logger.hh>
#include <Core/Assert.hh>

namespace Mikoto {
    auto Logger::Init() -> void {
        m_CoreLogger = spdlog::stdout_color_mt("KATE_CORE_LOGGER");
        m_AppLogger = spdlog::stdout_color_mt("KATE_APP_LOGGER");

        // Set m_CoreLogger pattern.
        // Check out the wiki for info about formatting
        // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
        m_CoreLogger->set_pattern("%^[%X] CORE LOG [thread %t] %v%$");

        // Log every message from the current level onwards.
        // If trace is used, all messages are logged including critical ones,
        // if debug is used, trace messages aren't logged and so on.
        m_CoreLogger->set_level(spdlog::level::trace);

        // Set m_AppLogger pattern.
        // Check out the wiki for info about formatting
        // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
        m_AppLogger->set_pattern("%^[%X] APP LOG [thread %t] %v%$");
        m_AppLogger->set_level(spdlog::level::trace);

        // Auto flush when "debug" or higher level message is logged on all loggers.
        // Check the FAQ for more about this matter.
        // https://github.com/gabime/spdlog/wiki/0.-FAQ
        spdlog::flush_on(spdlog::level::debug);
    }

    auto Logger::GetCoreLogger() -> const std::shared_ptr<spdlog::logger>& {
        KT_ASSERT(m_CoreLogger, "Core Logger is NULL");
        return m_CoreLogger;
    }

    auto Logger::GetAppLogger() -> const std::shared_ptr<spdlog::logger>& {
        KT_ASSERT(m_AppLogger, "Application Logger is NULL");
        return m_AppLogger;
    }
}