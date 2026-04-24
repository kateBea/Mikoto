//    Copyright 2025 ケイト
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

#ifndef MIKOTO_SCENE_RENDERER_HH
#define MIKOTO_SCENE_RENDERER_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/FrameGraph.hh>

#include <Renderer/Passes/DebugPasses.hh>

namespace mikoto::renderer {

    using namespace mikoto::scene;

    struct SceneRendererCreateInfo {
        GpuDevice* mDevice{};
        eastl::string_view mName{};
        eastl::string_view mShaderBasePath{};

        RenderResolution mTargetResolution{ RenderResolution::e1080P };

        auto WithName(eastl::string_view name) -> SceneRendererCreateInfo&;
        auto WithDevice(GpuDevice* device) -> SceneRendererCreateInfo&;
    };

    class SceneRenderer final : public Renderer {
    public:
        explicit SceneRenderer( const SceneRendererCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto Render( Scene* scene ) -> void override;

        auto SetMainCamera( SceneCamera* camera ) -> void;
        auto SetClearColor(const float4& color) -> void;

        MKT_NODISCARD auto GetTexture(eastl::string_view name) const -> TextureHandle;
        MKT_NODISCARD auto GetBuffer(eastl::string_view name) const -> BufferHandle;

        MKT_NODISCARD static auto Create( const SceneRendererCreateInfo& spec) -> eastl::unique_ptr<SceneRenderer>;

    private:
        GpuDevice* mDevice{};
        SceneCamera* mCamera{};

        RenderResolution mTargetResolution{ RenderResolution::e1080P };

        // Passes
        DebugPasses mDebugPasses{ mTargetResolution };

        eastl::unique_ptr<FrameGraph> mFrameGraph{};
    };
}// namespace Mikoto

#endif// MIKOTO_SCENE_RENDERER_HH
