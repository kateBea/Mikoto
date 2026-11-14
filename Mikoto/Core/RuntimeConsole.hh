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

#ifndef MIKOTO_CORE_RUNTIME_CONSOLE_HH
#define MIKOTO_CORE_RUNTIME_CONSOLE_HH

#include <mutex>

#include <EASTL/deque.h>
#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/functional.h>
#include <EASTL/string_view.h>

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
        ConsoleLogLevel mLevel{};
        eastl::string mMessage{};
    };

    struct ConsoleLogEntry {
        u64 mID{};
        ConsoleLogLevel mLevel{};
        eastl::string mText{};
    };

    struct RuntimeConsoleCreateInfo {
        usize mMaxLogEntries{ 10000 };
    };

    class RuntimeConsole final : public IService, public Singleton<RuntimeConsole> {
    public:
        struct CommandInfo {
            eastl::string mName{};
            eastl::string mDescription{};
        };

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
        MKT_NODISCARD auto GetCommands() const -> eastl::vector<CommandInfo>;

        auto Info(eastl::string_view message) -> void;
        auto Debug(eastl::string_view message) -> void;
        auto Error(eastl::string_view message) -> void;
        auto Warning(eastl::string_view message) -> void;

        auto AddLog(ConsoleMessage message) -> void;
        auto AddLog(ConsoleLogLevel level, eastl::string_view message) -> void;

        // Snapshot for occasional callers. Panels should consume incremental
        // updates instead of copying the entire history every frame.
        MKT_NODISCARD auto GetLogs() const -> eastl::vector<eastl::string>;

        // Copies only entries at/after nextID under the producer lock. Returns
        // the oldest retained ID so consumers can evict expired cached rows.
        auto ReadLogUpdates( u64& nextID, eastl::vector<ConsoleLogEntry>& entries ) const -> u64;

        /**
         * @brief Returns the message counts in the retained console history.
         * @returns Counts indexed by ConsoleLogLevel, independent of panel filters and visibility.
         */
        MKT_NODISCARD auto GetLogCounts() const -> eastl::array<usize, 4>;

    private:
        auto ClearLogs() -> void;
        auto RegisterDefaultCommands() -> void;

    private:
        eastl::string mName{};
        eastl::deque<ConsoleLogEntry> mLogEntries{};
        eastl::array<usize, 4> mLogCounts{};
        usize mMaxLogEntries{};
        u64 mNextLogID{ 1 };

        mutable std::mutex mLogRegisterMutex{};
        mutable std::mutex mCommandRegisterMutex{};
        ankerl::unordered_dense::map<eastl::string, Command> mRegisteredCommands{};
    };
}

#endif// MIKOTO_CORE_RUNTIME_CONSOLE_HH
