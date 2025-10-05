/**
 * EntryPoint.cc
 * Created by kate on 8/26/23.
 * */

// Project Headers
#include <EditorApp.hh>
#include <Library/Utility/Types.hh>

auto main( const int argc, char** argv ) -> int {
    using namespace Mikoto;

    EditorApp app{};
    const Int32 ret{ app.Run( argc, argv ) };

    return ret;
}