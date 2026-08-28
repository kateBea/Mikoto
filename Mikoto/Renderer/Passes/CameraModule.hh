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

#ifndef MIKOTO_CAMERA_PASS_HH
#define MIKOTO_CAMERA_PASS_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    struct CameraData {
        core::float4x4 mView{};
        core::float4x4 mProjection{};

        core::float4x4 mInverseProjection{};
        core::float4x4 mInverseViewProjection{}; // inverse(proj * view)

        core::float4 mCameraPosition{};

        core::float4 mPlaneBounds{}; // Near and far plane (x = zNear, y = zFar)
        core::float4 mScreenDimensions{}; // Width and Height (x = Width, y = Height)

        core::f32 mGamma{ 2.0f };
        core::f32 mExposure{ 1.0f };
    };

    struct CameraModuleInfo {
        FGBufferHandle mCameraData{};
    };

    class CameraModule {
    public:
        explicit CameraModule( rhi::RenderResolution resolution );

        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetGamma( core::f32 value ) -> void;
        auto SetExposure( core::f32 value ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

    private:
        auto RegisterCameraSetupPass( FrameGraph& graph ) -> void;

    private:

        CameraData mCameraData{};
        const scene::Camera* mCamera{};

        core::f32 mGamma{ 1.0f };
        core::f32 mExposure{ 1.0f };

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };
}// namespace mikoto::renderer


#endif // MIKOTO_CAMERA_PASS_HH
