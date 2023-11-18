//
// Created by kate on 11/11/23.
//

#ifndef MIKOTO_SANDBOX_HH
#define MIKOTO_SANDBOX_HH

#include <Common/Types.hh>

#include <Core/Application.hh>

namespace Mikoto {
    class Sandbox : public Application {
    public:
        auto Run(Int32_T argc, char** argv) -> Int32_T;

    private:
        auto ParseArguments(Int32_T argc, char** argv) -> void;

    private:
        std::vector<std::string> m_CommandLineArgs{}; /**< Holds the command line arguments. */
    };
}


#endif//MIKOTO_SANDBOX_HH
