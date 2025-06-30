/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

// Project Headers
#include <EditorApp.hh>
#include <Library/Utility/Types.hh>

auto main( const int argc, char** argv ) -> int {
    using namespace Mikoto;

    const auto app{ CreateScope<EditorApp>() };

    const Int32_T ret{ app->Run( argc, argv ) };

    return ret;
}