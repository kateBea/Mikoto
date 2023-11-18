//
// Created by kate on 11/11/23.
//

#include <Sandbox.hh>

namespace Mikoto {
    auto Sandbox::ParseArguments(Int32_T argc, char** argv) -> void {
        const auto limit{ std::addressof(argv[argc]) };
        for ( ; argv < limit; ++argv) {
            m_CommandLineArgs.emplace_back(*argv);
        }
    }

    auto Sandbox::Run(Int32_T argc, char** argv) -> Int32_T {
        ParseArguments(argc, argv);
    }
}
