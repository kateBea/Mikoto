/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Core/Engine.hh>

auto main(int argc, char** argv) -> int {
    auto app { std::make_unique<Mikoto::Engine>() };
    auto ret{ app->Run(argc, argv) };

    return ret;
}