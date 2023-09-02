/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

// Project Headers
#include <Core/Engine.hh>

int main(int argc, char** argv) {
    // Make Engine into namespace
    // Engine::Start(argc, argv);
    // Engine::Run();
    auto app { new Mikoto::Engine() };
    auto ret{ app->Run(argc, argv) };
    delete app;

    return ret;
}