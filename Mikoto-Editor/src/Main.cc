/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

// C++ Standard Library
#include <cstdlib>
#include <memory>

// Project Headers
#include <EditorApp.hh>

auto main( const int argc, char** argv ) -> int {
    using namespace Mikoto;

    EditorApp app{ };

    const auto ret{ app.Run( argc, argv ) };

    return ret;
}