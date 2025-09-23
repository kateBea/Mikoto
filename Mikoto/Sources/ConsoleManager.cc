// //
// // Created by kate on 10/12/23.
//
//
// #include <Timing/TimeService.hh>
//
// #include "Core/RuntimeConsole.hh"
//
// namespace Mikoto {
//     static constexpr auto GetLevelStr(ConsoleLogLevel level) -> std::string_view {
//         switch (level) {
//             case ConsoleLogLevel::CONSOLE_ERROR: return "ERROR";
//             case ConsoleLogLevel::CONSOLE_INFO: return "INFO";
//             case ConsoleLogLevel::CONSOLE_DEBUG: return "DEBUG";
//             case ConsoleLogLevel::CONSOLE_WARNING: return "WARNING";
//         }
//
//         return "";
//     }
//
//     auto RuntimeConsole::PushMessage(ConsoleLogLevel level, std::string_view message) -> void {
//
//         auto time{ TimeService::GetInstance()->GetTime(  ) };
//         m_Messages.emplace_back(level, fmt::format("[ {} ] [ {} ] {}", GetLevelStr(level), time, message));
//     }
//
//     auto RuntimeConsole::GetMessages() -> const std::vector<ConsoleMessage>& {
//         return m_Messages;
//     }
//
//     auto RuntimeConsole::ClearMessages() -> void {
//         m_Messages.clear();
//     }
//
// }