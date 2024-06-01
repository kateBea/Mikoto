//
// Created by kate on 10/12/23.

#include "../Tools/ConsoleManager.hh"

#include "Core/TimeManager.hh"
#include "fmt/format.h"

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