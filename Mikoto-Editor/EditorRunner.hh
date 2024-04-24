/**
 * Engine.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_EDITOR_RUNNER_HH
#define MIKOTO_EDITOR_RUNNER_HH

// C++ Standard Library
#include <vector>

// Project Headers
#include <Common/Types.hh>

namespace Mikoto {
    class EditorRunner {
    public:
        /**
         * @brief Creates and initializes the editor app and runs the main loop.
         * @param argc argument count.
         * @param argv list of null terminated c-strings command line arguments.
         * */
        auto Run(Int32_T argc, char** argv) -> Int32_T;

    private:
        /**
         * @brief Initializes the internal list of command line arguments.
         *
         * @param argc argument count.
         * @param argv list of null terminated c-strings command line arguments.
         * */
        auto ParseArguments(Int32_T argc, char **argv) -> void;

    private:
        std::vector<std::string> m_CommandLineArgs{}; /**< Holds the command line arguments. */
    };
}

#endif// MIKOTO_EDITOR_RUNNER_HH
