//    Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0 (the "License");

#ifndef MIKOTO_NETWORK_DEBUG_LAYER_HH
#define MIKOTO_NETWORK_DEBUG_LAYER_HH

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <Core/LayerStack.hh>
#include <Networking/HttpProbe.hh>

namespace mikoto::editor {

    class NetworkDebugLayer final : public core::ILayer {
    public:
        NetworkDebugLayer();

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float timeStep ) -> void override;
        auto OnEvent( core::IEvent& event ) -> void override;

    private:
        auto DisplayImGuiWindow() -> void;

        char mUrl[1024]{ "http://localhost:9000/" };
        bool mVisible{ true };
        eastl::unique_ptr<network::HttpProbe> mProbe{};
        eastl::string mBodyPreview{};
    };
}

#endif // MIKOTO_NETWORK_DEBUG_LAYER_HH
