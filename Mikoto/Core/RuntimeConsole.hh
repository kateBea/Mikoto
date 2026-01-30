//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_CONSOLE_MANAGER_HH
#define MIKOTO_CONSOLE_MANAGER_HH

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>
#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Common/Singleton.hh>

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

    /**
     * @brief A runtime console service that manages command registration and log output.
     * Used by the ConsolePanel to display logs and execute commands interactively.
     */
    class RuntimeConsole final : public IService, public Singleton<RuntimeConsole> {
    public:
        struct Command {
            std::string Name;
            std::string Description;
            std::function<void(const std::vector<std::string>& args)> Callback;
        };

        explicit RuntimeConsole(const ConsoleManagerCreateInfo& createInfo);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto RegisterCommand(
            const std::string& name,
            const std::string& desc,
            std::function<void(const std::vector<std::string>&)> callback
        ) -> void;

        auto ExecuteCommand(const std::string& input) -> void;

        auto Error(std::string_view message) -> void;
        auto Info(std::string_view message) -> void;
        auto Debug(std::string_view message) -> void;
        auto Warning(std::string_view message) -> void;

        auto AddLog(ConsoleMessage message) -> void;
        auto AddLog(ConsoleLogLevel level, std::string_view message) -> void;

        MKT_NODISCARD auto GetLogs() const -> const std::vector<std::string>& { return m_LogEntries; }

    private:
        std::unordered_map<std::string, Command> m_Commands;
        std::vector<std::string> m_LogEntries{};
        std::string m_Name{};
    };
}
#endif // MIKOTO_CONSOLE_MANAGER_HH
