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

#include <mutex>

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/functional.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

namespace mikoto::core {

    enum class ConsoleLogLevel {
        eError,
        eInfo,
        eDebug,
        eWarning,
    };

    struct ConsoleMessage {
        ConsoleLogLevel Level{};
        eastl::string mMessage{};
    };

    struct RuntimeConsoleCreateInfo {
    };

    class RuntimeConsole final : public IService, public Singleton<RuntimeConsole> {
    public:
        struct Command {
            eastl::string mName{};
            eastl::string mDescription{};
            eastl::function<void(const eastl::vector<eastl::string>& args)> mCallback{};
        };

        explicit RuntimeConsole(const RuntimeConsoleCreateInfo& createInfo);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto RegisterCommand(
            const eastl::string& name,
            const eastl::string& desc,
            eastl::function<void(const eastl::vector<eastl::string>&)> callback
        ) -> void;

        auto ExecuteCommand(const eastl::string& input) -> void;

        auto Info(eastl::string_view message) -> void;
        auto Debug(eastl::string_view message) -> void;
        auto Error(eastl::string_view message) -> void;
        auto Warning(eastl::string_view message) -> void;

        auto AddLog(ConsoleMessage message) -> void;
        auto AddLog(ConsoleLogLevel level, eastl::string_view message) -> void;

        MKT_NODISCARD auto GetLogs() const -> const eastl::vector<eastl::string>&;

    private:
        auto RegisterDefaultCommands() -> void;

    private:
        eastl::string m_Name{};
        eastl::vector<eastl::string> mLogEntries{};

        std::mutex mCommandRegisterMutex{};
        ankerl::unordered_dense::map<eastl::string, Command> mRegisteredCommands{};
    };
}
#endif // MIKOTO_CONSOLE_MANAGER_HH
