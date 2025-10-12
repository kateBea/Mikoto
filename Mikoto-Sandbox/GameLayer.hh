//
// Created by zanet on 10/10/2025.
//

#ifndef GAMELAYER_HH
#define GAMELAYER_HH

#include <imgui.h>

#include <Assets/Audio.hh>
#include <Core/LayerStack.hh>
#include <string_view>


namespace Mikoto {
    class GameLayer final : public ILayer {
    public:
        explicit GameLayer( std::string_view name );

        auto OnUpdate( float deltaTime ) -> void override;
        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;

    private:
        bool m_ShowDemo{ false };
        bool m_ShowAnotherWindow{ false };
        ImVec4 m_ClearColor{ ImVec4(0.45f, 0.55f, 0.60f, 1.00f) };
    };
}// namespace Mikoto


#endif//GAMELAYER_HH
