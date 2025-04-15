//
// Created by kate on 10/12/23.
//

#ifndef MIKOTO_CONSOLE_MANAGER_HH
#define MIKOTO_CONSOLE_MANAGER_HH

#include <vector>
#include <string>
#include <string_view>

#include <Library/Utility/Types.hh>
#include <Common/Service.hh>

namespace Mikoto {
    enum class ConsoleLogLevel {
        CONSOLE_ERROR,
        CONSOLE_INFO,
        CONSOLE_DEBUG,
        CONSOLE_WARNING,
    };

    struct ConsoleManagerCreateInfo {
        std::string_view Name{};
    };

    struct ConsoleMessage {
        ConsoleLogLevel Level{};
        std::string Message{};
    };

    class RuntimeConsole : public IService<RuntimeConsole> {
    public:
        explicit RuntimeConsole(const ConsoleManagerCreateInfo& createInfo);

        auto ClearMessages() -> void;
        auto GetMessages() -> const std::vector<ConsoleMessage>&;
        auto PushMessage(ConsoleLogLevel level, std::string_view message) -> void;

    private:
        std::vector<ConsoleMessage> m_Messages{};
    };
}

#endif // MIKOTO_CONSOLE_MANAGER_HH
