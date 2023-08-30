/**
 * Engine.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_ENGINE_HH
#define MIKOTO_ENGINE_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/Random.hh>

namespace Mikoto {
    class Engine {
    public:
        auto Run(Int32_T argc, char **argv) -> Int32_T;

    private:

    };
}


#endif // MIKOTO_ENGINE_HH
