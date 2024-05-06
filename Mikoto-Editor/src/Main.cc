/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

// C++ Standard Library
#include <cstdlib>
#include <memory>

// Project Headers
#include <EditorApp.hh>

auto main(int argc, char** argv) -> int {
    using namespace Mikoto;

    auto app { std::make_unique<EditorApp>() };
    auto ret{ app ? app->Run(argc, argv) : EXIT_FAILURE };

    return ret;
}