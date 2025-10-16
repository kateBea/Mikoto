/**
 * EntryPoint.cc
 * Created by kaTe on 12/11/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <SandboxApp.hh>

auto main(int argc, char** argv) -> int {
    using namespace Mikoto;

    SandboxApp app{};
    const auto ret{ app.Run(argc, argv) };

    return ret;
}