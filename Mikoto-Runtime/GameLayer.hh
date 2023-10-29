/**
 * GameLayer.hh
 * Created by kate on 10/6/23.
 * */

#ifndef MIKOTO_GAME_LAYER_HH
#define MIKOTO_GAME_LAYER_HH

// C++ Standard Library
#include <string_view>

// Project Headers
#include "Core/Layer.hh"

namespace Mikoto {
    class GameLayer : public Layer {
    public:
        explicit GameLayer(std::string_view name = "Game Layer")
            :   Layer{ name }
        {

        }

        auto OnAttach() -> void override;
        auto OnDetach() -> void override;
        auto OnUpdate(double ts) -> void override;
    };
}

#endif // MIKOTO_GAME_LAYER_HH
