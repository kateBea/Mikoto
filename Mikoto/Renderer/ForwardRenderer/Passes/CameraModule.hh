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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_CAMERA_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_CAMERA_MODULE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/FrameGraph.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

namespace mikoto::renderer {

    // https://www.sidefx.com/docs/houdini/solaris/kug/cameras.html
    // https://github.com/mmp/pbrt-v3/blob/master/src/cameras/realistic.cpp
    // https://www.pbr-book.org/3ed-2018/Camera_Models/Realistic_Cameras
    // https://www.pbr-book.org/3ed-2018/Camera_Models/Environment_Camera

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
        core::u32 mVisualizeCascades{};
        core::u64 mVolumes{};
        core::u64 mFogFrame{};

        core::u64 mAtmosphere{};
        core::u32 mAtmosphereTransmittance{};
        core::u32 mAtmosphereSampler{};
    };

    struct CameraModuleInfo {
        FGBufferHandle mCameraData{};
    };

    class CameraModule {
    public:
        explicit CameraModule( rhi::RenderResolution resolution );

        auto RegisterCameraSetupPass( FrameGraph& graph ) -> void;

        auto SetGamma( core::f32 value ) -> void;
        auto SetExposure( core::f32 value ) -> void;

        auto SetVisualizeCascades( bool enabled ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

    private:
        CameraData mCameraData{};
        const scene::Camera* mCamera{};

        core::f32 mGamma{ 1.0f };
        core::f32 mExposure{ 1.0f };

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_CAMERA_MODULE_HH

