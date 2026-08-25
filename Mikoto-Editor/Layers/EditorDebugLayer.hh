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

#ifndef MIKOTO_EDITOR_DEBUG_LAYER_HH
#define MIKOTO_EDITOR_DEBUG_LAYER_HH

#include <EASTL/array.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Event.hh>
#include <Core/LayerStack.hh>
#include <Platform/Window.hh>

#include <Assets/Model.hh>

#include <Scene/Camera.hh>
#include <Scene/SceneCamera.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::editor {

    // Debug layer that renders objects instanced
    // and feature bindless textures
    class EditorDebugLayer final : public core::ILayer {
    public:

        explicit EditorDebugLayer( platform::Window* window );

        auto OnUpdate( float deltaTime ) -> void override;
        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnEvent( core::IEvent &event ) -> void override;

    private:
        struct ConstantBuffer {
            core::float4x4 mModel{ math::constants::Identity<core::float4x4>() };
            core::float4x4 mView{};
            core::float4x4 mProjection{};

            core::i32 mTextureIndex{ 0 };
            core::i32 mSamplerIndex{ 0 };
        };

        auto UpdateCameraState( float ts ) -> void;

    private:
        renderer::IGpuDevice* mDevice{};

        asset::ModelHandle mModelHandle{};

        eastl::unique_ptr<scene::SceneCamera> mEditorCamera{};

        core::float3 mPosition{ 0.0f };
        core::float3 mScale{ 1.0f };
        core::float3 mRotation{ 0.0f };// degrees
        core::float3 mPivot{ 0.0, 0.1, 0.0f };
        ConstantBuffer mCameraProps{};
        renderer::rhi::BufferHandle mConstantBuffer{};

        renderer::rhi::TextureHandle mColorImage{};
        renderer::rhi::TextureHandle mDepthImage{};
        renderer::rhi::TextureHandle mSimpleTexture{};
        renderer::rhi::SamplerHandle mSamplerState{};

        renderer::rhi::InputLayoutHandle mVertexInputLayout{};

        renderer::rhi::ShaderModuleHandle mVertexShader{};
        renderer::rhi::ShaderModuleHandle mPixelShader{};

        renderer::rhi::CommandListHandle mCommandList{};

        renderer::rhi::PipelineHandle mPipeline{};
        renderer::rhi::BindingSetHandle mBindingSetHandle{};
        renderer::rhi::BindingLayoutHandle mBindlessLayout{};
        renderer::rhi::DescriptorTableHandle mDescriptorTable{};
        renderer::rhi::BindingLayoutHandle mBindingLayoutHandle{};
        renderer::rhi::PipelineLayoutHandle mPipelineLayoutHandle{};

        platform::Window* mWindow{};
    };

}

#endif//MIKOTO_EDITOR_DEBUG_LAYER_HH
