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

#pragma once

#include "Material_Helpers.slang"
#include "Memory_Helpers.slang"
#include "InfiniteGrid_Helpers.slang"

// Layouts mirror ThumbnailPasses::SceneData / DrawData.
struct PreviewScene {
    float4x4 mViewProjection;
    float4x4 mInverseViewProjection;
    float4x4 mLightViewProjection;
    float4 mCamera;
    float4 mLightDirection;
    float4 mLightColor;
    float4 mSettings;
    float4 mBloom;
    float4 mPreviewOptions;
};
struct PreviewDraw {
    float4x4 mModel;
    float4x4 mNormal;
    float4 mBaseColor;
    float4 mEmission;
    float4 mFactors;
    float4 mFlags;
    int4 mTextures0;
    int4 mTextures1;
    int4 mUv0;
    int4 mUv1;
    Pointer mVertices;
    Pointer mIndices;
    Pointer mPalette;
    uint mJointCount;
    uint mPadding;
    float4 mSurfaceShading;
    float4 mShadingModel;
    int mClearCoatNormalIndex;
    int mClearCoatNormalTextureSet;
    float mClearCoatNormalScale;
    float mCoatPadding;
    float4 mUvTransform;
    float4 mDielectricAttenuation;
};
struct PreviewParameters {
    Pointer mScene;
    Pointer mDraws;
    uint mDrawIndex;
    uint mSampler;
    uint mShadow;
    int mEnvironment;
    uint mMode;
    uint mColor;
    uint mClampSampler;
    uint mBloom;
    uint mPrefilter;
    uint mPrefilterMipCount;
};
struct PreviewVertex {
    float4 mPosition : SV_Position;
    float3 mWorld : TEXCOORD0;
    float3 mNormal : TEXCOORD1;
    float2 mUv0 : TEXCOORD2;
    float2 mUv1 : TEXCOORD3;
    float4 mColor : TEXCOORD4;
};
MKT_DECLARE_PUSH_CONSTANT( PreviewParameters, preview );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) previewTextures;
MKT_DECLARE_TEXTURE_SRV( TextureCube ) previewCubes;
MKT_DECLARE_SAMPLERS previewSamplers;

float2 PreviewUv( PreviewDraw draw, PreviewVertex vertex, int set ) {
    return ( set == 1 ? vertex.mUv1 : vertex.mUv0 ) * draw.mUvTransform.xy + draw.mUvTransform.zw;
}
float4 PreviewTexture( int id, float2 uv, float4 fallback ) {
    if ( id < 0 ) {
        return fallback;
    }
    return previewTextures[id].Sample( previewSamplers[preview.mSampler], uv );
}
float4 PreviewBaseColor( PreviewDraw draw, PreviewVertex vertex ) {
    if ( draw.mUv1.z == 2 || draw.mUv1.z == 3 ) {
        return float4( 1 );
    }
    float4 texel = PreviewTexture( draw.mTextures0.x, PreviewUv( draw, vertex, draw.mUv0.x ), float4( 1 ) );
    // Model color textures are uploaded as UNORM; decode color only. Normal,
    // roughness and metallic channels remain linear data.
    texel.rgb = pow( max( texel.rgb, 0.0 ), float3( 2.2 ) );
    return draw.mBaseColor * texel * vertex.mColor;
}

float3 PreviewSkyColor( PreviewDraw draw, float3 direction ) {
    direction = normalize( direction );
    uint face = 0;
    float2 uv;
    if ( draw.mUv1.z == 2 ) {
        uv = float2( atan2( direction.z, direction.x ) / ( 2.0 * 3.14159265 ) + 0.5,
                     acos( clamp( direction.y, -1.0, 1.0 ) ) / 3.14159265 );
    } else {
        float3 axis = abs( direction );
        if ( axis.x >= axis.y && axis.x >= axis.z ) {
            face = direction.x > 0 ? 0 : 1;
            uv = float2( direction.x > 0 ? -direction.z : direction.z, -direction.y ) / axis.x;
        } else if ( axis.y >= axis.z ) {
            face = direction.y > 0 ? 2 : 3;
            uv = float2( direction.x, direction.y > 0 ? direction.z : -direction.z ) / axis.y;
        } else {
            face = direction.z > 0 ? 4 : 5;
            uv = float2( direction.z > 0 ? direction.x : -direction.x, -direction.y ) / axis.z;
        }
        uv = uv * 0.5 + 0.5;
    }
    int id = face < 4 ? draw.mTextures0[face] : draw.mTextures1[face - 4];
    if ( id < 0 ) {
        return float3( 0.18 );
    }
    float3 color = previewTextures[id]
                           .Sample( previewSamplers[draw.mUv1.z == 2 ? preview.mSampler : preview.mClampSampler], uv )
                           .rgb;
    if ( ( draw.mPadding & ( 1u << face ) ) != 0 ) {
        color = pow( max( color, 0.0 ), float3( 2.2 ) );
    }
    return color * draw.mFactors.x;
}
float3 PreviewEnvironment( float3 direction, float lod = 0.0 ) {
    direction = normalize( direction );
    if ( preview.mEnvironment >= 0 ) {
        float2 uv = float2( atan2( direction.z, direction.x ) / ( 2.0 * 3.14159265 ) + 0.5,
                            acos( clamp( direction.y, -1.0, 1.0 ) ) / 3.14159265 );
        return max( previewTextures[preview.mEnvironment].SampleLevel( previewSamplers[preview.mSampler], uv, lod ).rgb,
                    0.0 );
    }
    // A neutral studio environment is always available, even before an HDR
    // asset is selected. Broad softboxes make roughness and reflections legible.
    float3 horizon =
            lerp( float3( 0.055, 0.065, 0.085 ), float3( 0.38, 0.43, 0.52 ), saturate( direction.y * 0.5 + 0.5 ) );
    float key = pow( saturate( dot( direction, normalize( float3( -1.0, 1.2, 0.8 ) ) ) ), 48.0 );
    float fill = pow( saturate( dot( direction, normalize( float3( 1.0, 0.5, -0.8 ) ) ) ), 18.0 );
    return horizon + key * float3( 5.0, 4.7, 4.3 ) + fill * float3( 1.5, 1.8, 2.2 );
}
float3 PreviewBlurredEnvironment( float3 direction, float roughness ) {
    // Roughness levels contain integrated radiance, so small HDR lights become
    // smooth lobes rather than the repeated dots of a sparse per-pixel blur.
    float lod = saturate( roughness ) * float( preview.mPrefilterMipCount - 1 );
    return previewCubes[preview.mPrefilter].SampleLevel(
        previewSamplers[preview.mClampSampler], normalize( direction ), lod ).rgb;
}

// Reuse the editor grid's antialiasing and scale transitions. Composite before
// transparent meshes, so particles/glass can blend over the reference plane.
float4 PreviewGrid( float3 world, float2 footprint, float3 eye ) {
    GridDrawParams grid = (GridDrawParams)0;
    grid.mMinorColor = float4( 0.45, 0.45, 0.45, 0.35 );
    grid.mMajorColor = float4( 0.65, 0.65, 0.65, 0.5 );
    grid.mXAxisColor = float4( 0.7, 0.15, 0.15, 0.65 );
    grid.mZAxisColor = float4( 0.15, 0.3, 0.7, 0.65 );
    grid.mWidths = float4( 1, 1.5, 1.5, 1.5 );
    grid.mGrid = float4( 1, 1, 0, 0 );
    float4 color = InfiniteGridColor( world.xz, footprint, grid );
    float3 ray = world - eye;
    float fade = ( 1 - smoothstep( 12.5, 45, length( ray ) ) ) *
                 smoothstep( 0.005, 0.05, abs( ray.y ) / max( length( ray ), 1e-6 ) );
    return color * fade;
}
