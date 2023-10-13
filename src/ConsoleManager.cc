//
// Created by kate on 10/12/23.

#include <fmt/format.h>

#include <Core/TimeManager.hh>
#include <Editor/ConsoleManager.hh>

namespace Mikoto {
    static constexpr auto GetLevelStr(ConsoleLogLevel level) -> std::string_view {
        switch (level) {
            case ConsoleLogLevel::ERROR: return "ERROR";
            case ConsoleLogLevel::INFO: return "INFO";
            case ConsoleLogLevel::DEBUG: return "DEBUG";
            case ConsoleLogLevel::WARNING: return "WARNING";
        }
    }

    auto ConsoleManager::PushMessage(ConsoleLogLevel level, std::string_view message) -> void {
        auto time{ TimeManager::ToString(TimeManager::GetTime()) };
        s_Messages.emplace_back(level, fmt::format("[ {} ] [ {} ] {}", GetLevelStr(level), time, message));
    }

    auto ConsoleManager::GetMessages() -> const std::vector<Node>& {
        return s_Messages;
    }

    auto ConsoleManager::ClearMessages() -> void {
        s_Messages.clear();
    }

}