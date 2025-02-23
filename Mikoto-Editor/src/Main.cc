/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

// C++ Standard Library
#include <cstdlib>
#include <memory>

// Project Headers
#include <EditorApp.hh>
#include <Common/Constants.hh>

auto main( const int argc, char** argv ) -> int {
    using namespace Mikoto;

    const auto app{ CreateScope<EditorApp>() };
    const auto ret{ app ? app->Run( argc, argv ) : EXIT_FAILURE };

    return ret;
}