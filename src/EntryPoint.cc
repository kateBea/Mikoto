/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Core/Engine.hh>

int main(int argc, char** argv) {
    auto app { std::make_shared<Mikoto::Engine>() };
    auto ret{ app->Run(argc, argv) };

    return ret;
}