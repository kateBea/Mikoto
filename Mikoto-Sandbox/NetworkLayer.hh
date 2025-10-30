//
// Created by kate on 10/30/25.
//

#ifndef NETWORK_LAYER_HH
#define NETWORK_LAYER_HH

#include <string_view>

#include <Assets/Audio.hh>
#include <Core/LayerStack.hh>
#include <Networking/Socket.hh>

namespace Mikoto {

    class NetworkLayer final : public ILayer {
    public:
        explicit NetworkLayer( std::string_view name );

        auto OnUpdate( float deltaTime ) -> void override;
        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;

        auto OnEvent(Event &event) -> void override;

    private:
        SocketHandle m_Socket{};
    };
}// namespace Mikoto



#endif //
