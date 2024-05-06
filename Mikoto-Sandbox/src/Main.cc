/**
 * EntryPoint.cc
 * Created by kaTe on 12/11/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <SandboxApp.hh>

auto main(int argc, char** argv) -> int {
    auto app { std::make_unique<Mikoto::SandboxApp>() };
    auto ret{ app->Run(argc, argv) };

    return ret;
}