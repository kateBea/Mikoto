/**
 * EntryPoint.cc
 * */

#include <Engine.hh>

int main(int, char**) {
    auto app { new Mikoto::Engine() };
    auto ret{app->Run() };
    delete app;

    return ret;
}