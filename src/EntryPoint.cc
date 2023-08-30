/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

// Project Headers
#include <Core/Engine.hh>

int main(int argc, char** argv) {
    auto app { new Mikoto::Engine() };

    auto ret{ app->Run(argc, argv) };

    delete app;

    return ret;
}