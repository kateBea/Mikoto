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

#ifndef MIKOTO_EDITOR_HELLO_TRIANGLE_LAYER_HH
#define MIKOTO_EDITOR_HELLO_TRIANGLE_LAYER_HH

#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Event.hh>
#include <Core/LayerStack.hh>

#include <Assets/Model.hh>

#include <Platform/Window.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/Core/ThumbnailRenderer.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>

#include <Panels/Panel.hh>

#include <Theme/Theme.hh>

namespace mikoto::editor {

    // The hello world of Graphics programming,
    // features a simple color interpolated triangle
    // No passing resources, everything is handled by the shader
    class EditorHelloTriangleLayer final : public core::ILayer {
    public:
        explicit EditorHelloTriangleLayer( platform::Window *window );

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float timeStep ) -> void override;

        auto OnEvent( core::IEvent &event ) -> void override;

    private:
        auto DisplayImGuiWindow() -> void;

    private:
        renderer::IGpuDevice* mDevice{};

        renderer::rhi::TextureHandle mColorImage{};
        renderer::rhi::TextureHandle mDepthImage{};

        renderer::rhi::ShaderModuleHandle mVertexShader{};
        renderer::rhi::ShaderModuleHandle mPixelShader{};

        renderer::rhi::CommandListHandle mCommandList{};

        renderer::rhi::PipelineHandle mPipeline{};

        bool mIsImguiWindowActive{ false };

        platform::Window* mWindow{};
    };
}// namespace mikoto::editor

#endif//MIKOTO_EDITOR_HELLO_TRIANGLE_LAYER_HH
