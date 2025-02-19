//
// Created by kate on 11/11/23.
//

#ifndef MIKOTO_SANDBOX_APP_HH
#define MIKOTO_SANDBOX_APP_HH

#include <Library/Utility/Types.hh>
#include <Common/Application.hh>

namespace Mikoto {
    class SandboxApp : public Application {
    public:
        /**
         * @brief Creates and initializes the editor app and runs the main loop.
         * @param argc argument count.
         * @param argv list of null terminated c-strings command line arguments.
         * */
        auto Run(Int32_T argc, char** argv) -> Int32_T;

    protected:
        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto UpdateState() -> void override;

        auto InstallEventCallbacks() -> void;

    private:
        /**
         * @brief Initializes the internal list of command line arguments.
         * @param argc argument count.
         * @param argv list of null terminated c-string.
         * */
        auto ParseArguments(Int32_T argc, char **argv) -> void;

    private:
        std::vector<std::string> m_CommandLineArgs{}; /**< Holds the command line arguments. */
    };
}


#endif//MIKOTO_SANDBOX_APP_HH
