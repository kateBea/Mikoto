//
// Created by kate on 10/12/23.


#include <Core/Engine.hh>
#include <Core/System/TimeSystem.hh>
#include <Panels/ConsolePanel.hh>
#include <Tools/ConsoleManager.hh>

namespace Mikoto {
    static constexpr auto GetLevelStr(ConsoleLogLevel level) -> std::string_view {
        switch (level) {
            case ConsoleLogLevel::CONSOLE_ERROR: return "ERROR";
            case ConsoleLogLevel::CONSOLE_INFO: return "INFO";
            case ConsoleLogLevel::CONSOLE_DEBUG: return "DEBUG";
            case ConsoleLogLevel::CONSOLE_WARNING: return "WARNING";
        }

        return "";
    }

    auto ConsoleManager::PushMessage(ConsoleLogLevel level, std::string_view message) -> void {
        TimeSystem& timeSystem{ Engine::GetSystem<TimeSystem>() };

        auto time{ timeSystem.ToString(timeSystem.GetTime()) };
        s_Messages.emplace_back(level, fmt::format("[ {} ] [ {} ] {}", GetLevelStr(level), time, message));
    }

    auto ConsoleManager::GetMessages() -> const std::vector<ConsoleMessage>& {
        return s_Messages;
    }

    auto ConsoleManager::ClearMessages() -> void {
        s_Messages.clear();
    }

}