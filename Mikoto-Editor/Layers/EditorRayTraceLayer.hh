//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_LAYERS_EDITOR_RAY_TRACE_LAYER_HH
#define MIKOTO_LAYERS_EDITOR_RAY_TRACE_LAYER_HH

#include <EASTL/string.h>

#include <Core/Layer.hh>

#include <Platform/Window.hh>

#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::editor {
    class EditorRayTraceLayer final : public core::ILayer {
    public:
        explicit EditorRayTraceLayer( platform::IWindow* window );
        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float timeStep ) -> void override;
        auto OnRender( float deltaTime ) -> void override;
        auto OnEvent( platform::Event& event ) -> void override;

        MKT_NODISCARD auto GetName() const -> eastl::string_view override;

    private:
        auto CreateScene() -> void;
        auto Resize( core::u32 width, core::u32 height ) -> void;

        eastl::string mName{};

        platform::IWindow* mWindow{};
        renderer::rhi::IGpuDevice* mDevice{};
        renderer::rhi::BufferHandle mVertices{};
        renderer::rhi::BufferHandle mIndices{};
        renderer::rhi::AccelStructureHandle mBottomLevel{};
        renderer::rhi::AccelStructureHandle mTopLevel{};
        renderer::rhi::RayTracingPipelineHandle mPipeline{};
        renderer::rhi::ShaderTableHandle mShaderTable{};
        renderer::rhi::BindingLayoutHandle mGeometryLayout{};
        renderer::rhi::BindingTableHandle mGeometryBindings{};
        renderer::rhi::DescriptorTableHandle mSceneTable{};
        renderer::rhi::DescriptorTableIndex mSceneIndex{ renderer::rhi::kInvalidBindingItemIndex };

        renderer::rhi::CommandListHandle mCommands{};
        renderer::rhi::TextureHandle mOutput{};
        eastl::string mError{};
        bool mBuildPending{ true };
        bool mOrbit{ true };
        bool mShadows{ true };
        bool mReflections{ true };
        float mAngle{ 0.6f };
        float mDistance{ 8.0f };
        float mExposure{ 1.0f };
        float mFieldOfView{ 50.0f };
    };
}

#endif// MIKOTO_LAYERS_EDITOR_RAY_TRACE_LAYER_HH

