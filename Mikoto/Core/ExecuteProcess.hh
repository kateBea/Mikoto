//
// Created by kate on 10/28/25.
//

#ifndef EXECUTE_PROCESS_HH
#define EXECUTE_PROCESS_HH

#include <Common/Common.hh>
#include <functional>
#include <string>
#include <vector>

namespace Mikoto {

    /**
     * @brief Utility class to execute external processes and capture their output.
     * Used by the RuntimeConsole to run system commands or external tools.
     */
    class ExecuteProcess final {
    public:
        /**
         * @brief Executes a command and captures both stdout and stderr.
         * @param command Command string to execute.
         * @return Combined output (stdout + stderr) as a string.
         */
        static auto Run(const std::string& command) -> std::string;

        /**
         * @brief Executes a command without capturing output.
         * @param command Command string to execute.
         * @return Exit code of the process.
         */
        static auto RunDetached(const std::string& command) -> int;

        // Run asynchronously, invoking callback(line) for each output line
        static auto RunAsync(const std::string& command, std::function<void( const std::string& )> onOutput) -> void;
    };

} // namespace Mikoto


#endif //
