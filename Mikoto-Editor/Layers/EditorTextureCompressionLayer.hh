// Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0 (the "License").

#ifndef MIKOTO_LAYERS_EDITOR_TEXTURE_COMPRESSION_LAYER_HH
#define MIKOTO_LAYERS_EDITOR_TEXTURE_COMPRESSION_LAYER_HH

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <Core/Layer.hh>

namespace mikoto::editor {

    /**
     * @brief CPU benchmark for the production BC7/BC6H texture import path.
     * @details Scans Resources/Textures and Resources/Cubemaps. Runs one image
     * at a time on an owned worker, independently of project compression settings.
     * Reports source decode, encoding, KTX2 write/read and payload sizes. No GPU
     * work is timed. F9 toggles the window; shutdown joins the current codec stage.
     */
    class EditorTextureCompressionLayer final : public core::ILayer {
    public:
        EditorTextureCompressionLayer();
        ~EditorTextureCompressionLayer() override;

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float timeStep ) -> void override;
        auto OnRender( float deltaTime ) -> void override;
        auto OnEvent( platform::Event& event ) -> void override;

        MKT_NODISCARD auto GetName() const -> eastl::string_view override;

    private:
        auto ScanResources() -> void;
        auto StartBenchmark() -> void;
        auto DisplayWindow() -> void;

        struct State;
        eastl::string mName{};

        eastl::unique_ptr<State> mState{};
        bool mVisible{ true };
        core::i32 mIterations{ 1 };
    };
}

#endif// MIKOTO_LAYERS_EDITOR_TEXTURE_COMPRESSION_LAYER_HH

