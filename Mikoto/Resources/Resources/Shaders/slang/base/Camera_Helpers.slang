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

// CPU-mirrored camera matrices and shared world-to-clip projection. The explicit backend
// correction keeps raster passes consistent with the viewport coordinate convention.

#pragma once

struct CameraInfo {
    float4x4 mViewMatrix;
    float4x4 mProjection;

    float4x4 mInverseProjection;
    float4x4 mInverseViewProjection;

    float4 mViewPosition;
    float4 mPlanes;
    float4 mScreenDimensions;

    float mGamma;
    float mExposure;
    uint mVisualizeCascades;
    uint64_t mVolumes;
    uint64_t mFogFrame;

    uint64_t mAtmosphere;
    uint mAtmosphereTransmittance;
    uint mAtmosphereSampler;
    uint64_t mClouds;
};

// Depth prepass and shaded passes must agree down to the last depth bit.
// Prevent pass-specific contraction/reassociation of the projection arithmetic.
float4 CameraClipPosition( float4 worldPosition, CameraInfo camera ) {
    precise float4 viewPosition = mul( camera.mViewMatrix, worldPosition );
    precise float4 clipPosition = mul( camera.mProjection, viewPosition );
    return clipPosition;
}
