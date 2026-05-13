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

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    struct CameraInfo {
        float4x4 mView{};
        float4x4 mProjection{};

        float4x4 mInverseProjection{};
        float4x4 mInverseViewProjection{}; // inverse(proj * view)

        float4 mCameraPosition{};

        float4 mPlaneBounds{}; // Near and far plane (x = zNear, y = zFar)
        float4 mScreenDimensions{}; // Width and Height (x = Width, y = Height)
    };

    struct CameraModuleInfo {
        FGBufferHandle mCameraData{};
    };

    class CameraModule {
    public:
        explicit CameraModule(RenderResolution resolution);

        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetCamera( const scene::Camera* camera ) -> void;

    private:

        auto RegisterCameraSetupPass( FrameGraph& graph ) -> void;

    private:
        const scene::Camera* mCamera{};
        CameraInfo mCameraParameters{};
        RenderResolution mResolution{};
    };
}


#endif // MIKOTO_CAMERA_PASS_HH
