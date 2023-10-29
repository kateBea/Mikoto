/**
 * Engine.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_ENGINE_HH
#define MIKOTO_ENGINE_HH

// C++ Standard Library
#include <vector>

// Project Headers
#include "Common/Types.hh"
#include "Application.hh"

namespace Mikoto {
    class Engine {
    public:
        /**
         * @brief Initializes the engine subsystems and runs the main loop.
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

#endif // MIKOTO_ENGINE_HH
