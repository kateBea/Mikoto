/**
 * EntryPoint.cc
 * */

#include <Engine.hh>

int main(int, char**) {
    auto app { new kaTe::Engine() };
    auto ret{app->Run() };
    delete app;

    return ret;
}