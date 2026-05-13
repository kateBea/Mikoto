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

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/DebugModule.hh>
#include <Renderer/Passes/GeometryCullModule.hh>
#include <Renderer/Passes/GeometryShadingModule.hh>
#include <Renderer/Passes/MaterialModule.hh>
#include <Renderer/Passes/ParticleSimulationModule.hh>
#include <Renderer/Passes/PathTracingModule.hh>
#include <Renderer/Passes/PostProcessModule.hh>
#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/PresentationModule.hh>
#include <Renderer/Passes/RayTracingModule.hh>
#include <Renderer/Passes/ShadowMappingModule.hh>
#include <Renderer/Passes/TextRenderModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::scene;

    struct SceneRendererCreateInfo {
        GpuDevice* mDevice{};
        eastl::string_view mName{};
        eastl::string_view mShaderBasePath{};

        // Scene renderer does a full quad render on these
        // Just edit the final pass to specify what gets rendered onto these
        eastl::vector<TextureHandle> mPresentTextures{};

        RenderResolution mTargetResolution{ RenderResolution::e1080P };

        auto SetName(eastl::string_view name) -> SceneRendererCreateInfo&;
        auto SetDevice(GpuDevice* device) -> SceneRendererCreateInfo&;
        auto AddPresentImage(TextureHandle texture) -> SceneRendererCreateInfo&;
    };

    class SceneRenderer final : public Renderer {
    public:
        explicit SceneRenderer( const SceneRendererCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto Render( const Scene* scene ) -> void override;

        auto SetMainCamera( const SceneCamera* camera ) -> void;
        auto SetClearColor(const Color& color) -> void;

        MKT_NODISCARD auto GetRenderGraph() const -> const FrameGraph&;

        MKT_NODISCARD auto GetTexture( FGTextureHandle handle ) const -> TextureHandle;
        MKT_NODISCARD auto GetBuffer( FGBufferHandle handle ) const -> BufferHandle;

        MKT_NODISCARD static auto Create( const SceneRendererCreateInfo& spec) -> eastl::unique_ptr<SceneRenderer>;

    private:
        GpuDevice* mDevice{};
        SceneCamera* mCamera{};

        RenderResolution mTargetResolution{ RenderResolution::e1080P };

        // Passes
        CameraModule mCameraPass{ mTargetResolution };
        DebugModule mDebugPasses{ mTargetResolution };
        PrepassModule mRenderPrepass{ mTargetResolution };

        PresentationModule mPresentationModule{};
        GeometryCullModule mGeometryManagement{};
        GeometryShadingModule mGeometryShading{ mTargetResolution };

        MaterialModule mMaterialModule{ mTargetResolution };
        ShadowMappingModule mShadowMapping{ mTargetResolution };

        PostEffectsPass mPostEffectsPasses{ mTargetResolution };

        ParticleSimulationModule mParticleRendering{ mTargetResolution };

        TextRenderModule mTextRendering{ mTargetResolution };

        // Raytracing
        PathTracingModule mPathTracing{};
        RayTracingModule mRayTracingPass{};

        eastl::unique_ptr<FrameGraph> mFrameGraph{};
        eastl::unique_ptr<ShaderLibrary> mShaderLibrary{};
    };
}// namespace Mikoto

#endif// MIKOTO_SCENE_RENDERER_HH
