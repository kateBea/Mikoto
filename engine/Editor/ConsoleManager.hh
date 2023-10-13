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
        ERROR,
        INFO,
        DEBUG,
        WARNING,
    };

    class ConsoleManager {
    public:
        struct Node {
            ConsoleLogLevel Level{};
            std::string Message{};
        };

        static auto PushMessage(ConsoleLogLevel level, std::string_view message) -> void;
        static auto ClearMessages() -> void;

        static auto GetMessages() -> const std::vector<Node>&;

    private:

        inline static std::vector<Node> s_Messages{};
    };
}


#endif // MIKOTO_CONSOLE_MANAGER_HH
