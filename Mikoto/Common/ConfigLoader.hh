//
// Created by kate on 1/4/25.
//

#ifndef CONFIGLOADER_HH
#define CONFIGLOADER_HH

#include <toml++/toml.hpp>

#include <STL/Utility/Types.hh>

#include "Models/ConfigOptions.hh"

namespace Mikoto {
    class ConfigOptions {

    };
    
    class ConfigLoader final {
    public:
        static auto LoadFromFile(const Path_T& path) -> void {

        }

    private:
        static ConfigOptions s_Options{};
    };
}
#endif //CONFIGLOADER_HH
