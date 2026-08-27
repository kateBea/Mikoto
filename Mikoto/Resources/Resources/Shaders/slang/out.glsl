#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
layout(row_major) uniform;
layout(row_major) buffer;

#line 24 0
struct ClusteredParams_natural_0
{
    vec4 mGridSize_0;
    uint64_t mClustersPtr_0;
    uint64_t mCameraInfoPtr_0;
};


#line 27
layout(push_constant)
layout(std430) uniform block_ClusteredParams_natural_0
{
    vec4 mGridSize_0;
    uint64_t mClustersPtr_0;
    uint64_t mCameraInfoPtr_0;
}parameters_0;

#line 84 1
vec3 ScreenToView_0(vec2 screenCoord_0, vec2 screenDimensions_0, mat4x4 viewInverseProj_0)
{


    vec4 v_0 = (((vec4(screenCoord_0 / screenDimensions_0 * 2.0 - 1.0, 0.0, 1.0)) * (viewInverseProj_0)));


    return (v_0 / v_0.w).xyz;
}


#line 76
vec3 LineIntersectionWithZPlane_0(vec3 startPoint_0, vec3 endPoint_0, float zDistance_0)
{

#line 77
    vec3 direction_0 = endPoint_0 - startPoint_0;
    const vec3 normal_0 = vec3(0.0, 0.0, -1.0);


    return startPoint_0 + (zDistance_0 - dot(normal_0, startPoint_0)) / dot(normal_0, direction_0) * direction_0;
}


#line 31 2
struct CameraInfo_0
{
    mat4x4 mViewMatrix_0;
    mat4x4 mProjection_0;
    mat4x4 mInverseProjection_0;
    mat4x4 mInverseViewProjection_0;
    vec4 mViewPosition_0;
    vec4 mPlanes_0;
    vec4 mScreenDimensions_0;
};

layout(buffer_reference, std430, buffer_reference_align = 4) buffer BufferPointer_CameraInfo_0_1
{
    CameraInfo_0 _data;
};

#line 38 1
struct Cluster_0
{
    vec4 mCenter_0;
    vec4 mClosestPoint_0;
    vec4 mDistanceSquared_0;
    vec4 mMinPoint_0;
    vec4 mMaxPoint_0;
    uint mCount_0;
    uint  mLightIndices_0[256];
};

layout(buffer_reference, std430, buffer_reference_align = 4) buffer BufferPointer_Cluster_0_2
{
    Cluster_0 _data;
};
layout(buffer_reference, std430, buffer_reference_align = 4) buffer BufferPointer__S3_4
{
    vec4 _data;
};

#line 31 0
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main()
{

#line 39
    float _S5 = parameters_0.mGridSize_0.x;
    uint _S6 = gl_WorkGroupID.z;

    vec2 _S7 = (BufferPointer_CameraInfo_0_1(parameters_0.mCameraInfoPtr_0))._data.mScreenDimensions_0.xy;

#line 42
    vec2 tileSize_0 = _S7 / parameters_0.mGridSize_0.xy;


    uvec2 _S8 = gl_WorkGroupID.xy;



    vec3 minTile_0 = ScreenToView_0(vec2(_S8) * tileSize_0, _S7, (BufferPointer_CameraInfo_0_1(parameters_0.mCameraInfoPtr_0))._data.mInverseProjection_0);
    vec3 maxTile_0 = ScreenToView_0(vec2(_S8 + 1U) * tileSize_0, _S7, (BufferPointer_CameraInfo_0_1(parameters_0.mCameraInfoPtr_0))._data.mInverseProjection_0);


    float nearZ_0 = (BufferPointer_CameraInfo_0_1(parameters_0.mCameraInfoPtr_0))._data.mPlanes_0.y;
    float farZ_0 = (BufferPointer_CameraInfo_0_1(parameters_0.mCameraInfoPtr_0))._data.mPlanes_0.x;

    float _S9 = parameters_0.mGridSize_0.z;

#line 56
    float planeNear_0 = nearZ_0 * pow(farZ_0 / nearZ_0, float(_S6) / _S9);
    float planeFar_0 = nearZ_0 * pow(farZ_0 / nearZ_0, float(_S6 + 1U) / _S9);

    const vec3 cameraPosViewSpace_0 = vec3(0.0, 0.0, 0.0);

#line 64
    vec3 maxPointNear_0 = LineIntersectionWithZPlane_0(cameraPosViewSpace_0, maxTile_0, planeNear_0);
    vec3 maxPointFar_0 = LineIntersectionWithZPlane_0(cameraPosViewSpace_0, maxTile_0, planeFar_0);

#line 79
    (BufferPointer_Cluster_0_2(parameters_0.mClustersPtr_0) + (gl_WorkGroupID.x + gl_WorkGroupID.y * uint(_S5) + _S6 * uint(_S5 * parameters_0.mGridSize_0.y)))._data.mMinPoint_0 = vec4(min(LineIntersectionWithZPlane_0(cameraPosViewSpace_0, minTile_0, planeNear_0), LineIntersectionWithZPlane_0(cameraPosViewSpace_0, minTile_0, planeFar_0)), 0.0);
    (BufferPointer_Cluster_0_2(parameters_0.mClustersPtr_0) + (gl_WorkGroupID.x + gl_WorkGroupID.y * uint(_S5) + _S6 * uint(_S5 * parameters_0.mGridSize_0.y)))._data.mMaxPoint_0 = vec4(max(maxPointNear_0, maxPointFar_0), 0.0);
    return;
}

