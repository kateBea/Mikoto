//
// Created by kate on 10/30/25.
//

#ifndef NETWORK_LAYER_HH
#define NETWORK_LAYER_HH

#include <vector>
#include <string_view>

#include <ankerl/unordered_dense.h>

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
        auto DrawAnimeWindow() -> void;

    private:
        struct AnimeInfo {
            Int32 id{};
            std::string title{};
        };

    private:
        SocketHandle m_LocalHostSocket{};

        bool m_HasFetchedList{ false };
        Int32 m_SelectedAnimeIndex{};

        std::vector<AnimeInfo> m_AnimeList{};
        ankerl::unordered_dense::map<Int32, std::string> m_SelectedAnimeJsons{};


    };
}// namespace Mikoto



#endif //
