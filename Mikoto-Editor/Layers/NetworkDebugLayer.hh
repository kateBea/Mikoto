//    Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0 (the "License");

#ifndef MIKOTO_LAYERS_NETWORK_DEBUG_LAYER_HH
#define MIKOTO_LAYERS_NETWORK_DEBUG_LAYER_HH

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <Core/Layer.hh>

#include <Networking/Client.hh>

namespace mikoto::editor {

    class NetworkDebugLayer final : public core::ILayer {
    public:
        NetworkDebugLayer();

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float timeStep ) -> void override;
        auto OnRender( float deltaTime ) -> void override;
        auto OnEvent( platform::Event &event ) -> void override;

        MKT_NODISCARD auto GetName() const -> eastl::string_view override;

    private:
        auto StartRequest() -> void;

        auto DisplayImGuiWindow() -> void;

        eastl::string mName{};

        char mUrl[1024]{ "http://localhost:9000/" };
        bool mVisible{ true };

        eastl::unique_ptr<network::HttpClient> mClient{};
        eastl::string mRequestUrl{};
        bool mResponseReady{};
        eastl::string mBodyPreview{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_LAYERS_NETWORK_DEBUG_LAYER_HH
