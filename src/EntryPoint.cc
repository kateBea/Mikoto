/**
 * EntryPoint.cc
 * */

#include <Engine.hh>

int main(int, char**) {
    // TODO: pass commandline arguments to the engine
    auto app { new Mikoto::Engine() };
    auto ret{ app->Run() };
    delete app;

    return ret;
}