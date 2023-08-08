/**
 * Sandbox.hh
 * Created by kate on 6/7/23.
 * */
#ifndef KATE_ENGINE_ENGINE_HH
#define KATE_ENGINE_ENGINE_HH

#include <memory>

#include <Utility/Common.hh>

namespace kaTe {
    class Engine {
    public:
        auto Run() -> kaTe::Int32_T;
    };
}


#endif//KATE_ENGINE_ENGINE_HH
