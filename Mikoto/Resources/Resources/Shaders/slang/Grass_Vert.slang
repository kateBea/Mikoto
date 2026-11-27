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

// Expand generated blade records or pull imported tuft triangles. Wind and interaction deform
// both paths before their world-space attributes are projected for color and contact-depth
// rendering.

#include "base/Grass_Helpers.slang"

struct GrassVaryings {
    float3 mWorld : TEXCOORD0;
    float3 mNormal : TEXCOORD1;
    float3 mTangent : TEXCOORD2;
    float4 mShape : TEXCOORD3;
    float4 mPosition : SV_Position;
};

[shader( "vertex" )]
GrassVaryings main( uint vertex : SV_VertexID, uint instance : SV_InstanceID ) {
    GrassDescription source = LoadBufferValue<GrassDescription>( parameters.mDescriptions, parameters.mTerrain * sizeof( GrassDescription ) );
    GrassBlade blade = LoadBufferValue<GrassBlade>( parameters.mBlades,
        ( source.mRanges.x + parameters.mLod * source.mRanges.y + instance ) * sizeof( GrassBlade ) );
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    float t;
    float side;
    float textureY;
    float3 rest = 0;
    float3 restNormal = float3( 0, 0, 1 );
    if ( source.mMesh.y != 0 ) {
        GrassMeshVertex mesh = LoadBufferValue<GrassMeshVertex>( parameters.mMeshes, ( source.mMesh.x + vertex ) * sizeof( GrassMeshVertex ) );
        rest = mesh.mPosition.xyz;
        restNormal = mesh.mNormal.xyz;
        // Texture atlases need arbitrary UVs. Wind deformation follows the
        // physical height so petals cannot pull their stems out of the ground.
        t = saturate( rest.y );
        textureY = mesh.mPosition.w;
        side = mesh.mNormal.w * 2 - 1;
    }
    else {
        // The high strip smoothly folds onto the low strip before the draw
        // switches from 15 to 7 vertices. Recover the surface root used by the
        // generator so its geometry cutoff and this morph use the same distance.
        float scale = length( mul( source.mWorld, float4( 0, 1, 0, 0 ) ).xyz );
        float3 surfaceRoot = blade.mRootHeight.xyz + blade.mNormalColor.xyz * .015 * scale;
        float distance = length( surfaceRoot - camera.mViewPosition.xyz );
        float morph = smoothstep( source.mView.y, max( source.mView.z, source.mView.y + .001 ), distance );
        GrassBladeCoordinates( vertex, parameters.mLod, morph, t, side );
        textureY = t;
    }
    float3 up = normalize( lerp( float3( 0, 1, 0 ), blade.mNormalColor.xyz, .3 ) );
    float3 facing = float3( blade.mFacingWidthBend.x, 0, blade.mFacingWidthBend.y );
    float3 wind = float3( blade.mWind.x, 0, blade.mWind.y ) * blade.mWind.z;
    float3 center;
    float3 tangent;
    GrassFlowCurve( t, blade.mRootHeight.w, blade.mFacingWidthBend.w, up, facing, wind, blade.mWind.w, center, tangent );
    float3 right = GrassFlowNormalize( cross( tangent, facing ), float3( 1, 0, 0 ) );
    float3 forward = normalize( cross( right, tangent ) );
    float3 normal = forward;
    float3 world = blade.mRootHeight.xyz + center;
    if ( source.mMesh.y != 0 ) {
        world += ( right * rest.x + forward * rest.z ) * blade.mFacingWidthBend.z;
        world += up * ( rest.y - t ) * blade.mRootHeight.w;
        float widthScale = max( blade.mFacingWidthBend.z, .0001 );
        float heightScale = max( blade.mRootHeight.w, .0001 );
        normal = GrassFlowNormalize( ( right * restNormal.x + forward * restNormal.z ) / widthScale + tangent * restNormal.y / heightScale, forward );
    }
    else {
        float taper = max( .001, 1 - t );
        world += right * side * blade.mFacingWidthBend.z * .5 * taper;
    }
    // Thicken edge-on ribbons in the camera plane as in GrassFlow. Imported
    // tufts retain their authored silhouette instead of being billboarded.
    if ( source.mMesh.y == 0 ) {
        float3 view = GrassFlowNormalize( camera.mViewPosition.xyz - world, forward );
        float edge = 1 - abs( dot( forward, view ) );
        float3 cameraRight = float3( camera.mViewMatrix[0][0], camera.mViewMatrix[0][1], camera.mViewMatrix[0][2] );
        world += cameraRight * side * blade.mFacingWidthBend.z * .25 * edge * edge * ( 1 - t );
    }
    GrassVaryings output;
    output.mWorld = world;
    output.mNormal = normal;
    output.mTangent = tangent;
    output.mShape = float4( t, side * .5 + .5, blade.mNormalColor.w, textureY );
    output.mPosition = CameraClipPosition( float4( world, 1 ), camera );
    return output;
}
