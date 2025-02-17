//
// Created by kate on 10/12/23.
//

#ifndef MIKOTO_CONSOLE_MANAGER_HH
#define MIKOTO_CONSOLE_MANAGER_HH

#include <vector>
#include <string>
#include <string_view>

namespace Mikoto {
    enum class ConsoleLogLevel {
        CONSOLE_ERROR,
        CONSOLE_INFO,
        CONSOLE_DEBUG,
        CONSOLE_WARNING,
    };

    struct ConsoleMessage {
        ConsoleLogLevel Level{};
        std::string Message{};
    };

    class ConsoleManager {
    public:
        static auto ClearMessages() -> void;
        static auto GetMessages() -> const std::vector<ConsoleMessage>&;
        static auto PushMessage(ConsoleLogLevel level, std::string_view message) -> void;

    private:
        inline static std::vector<ConsoleMessage> s_Messages{};
    };
}

#endif // MIKOTO_CONSOLE_MANAGER_HH
