/**
 * Logger.cc
 * Created by kate on 5/25/23.
 * */

// C++ Standard Library
#include <memory>

#include "spdlog/sinks/stdout_color_sinks.h"

// Project Headers
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>


namespace Mikoto {

    // Global logger instance
    Logger s_Logger{};

    Logger::Logger()
        : m_CoreLogger{ nullptr }
    {
        Init();
    }

    auto Logger::Init() -> void {
        m_CoreLogger = spdlog::stdout_color_mt("MIKOTO_CORE_LOGGER");

        // Set m_CoreLogger pattern.
        // Check out the wiki for info about formatting
        // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
        m_CoreLogger->set_pattern("%^[%X] CORE LOG [thread %t] %v%$");

        // Log every message from the current level onwards.
        // If trace is used, all messages are logged including critical ones,
        // if debug is used, trace messages aren't logged and so on.
        m_CoreLogger->set_level(spdlog::level::trace);

        // Auto flush when "debug" or higher level message is logged on all loggers.
        // Check the FAQ for more about this matter.
        // https://github.com/gabime/spdlog/wiki/0.-FAQ
        spdlog::flush_on(spdlog::level::debug);
    }

    auto Logger::GetConsoleLog() -> const Shared<spdlog::logger>& {
        MKT_ASSERT(m_CoreLogger, "Core Logger is NULL");
        return m_CoreLogger;
    }
}