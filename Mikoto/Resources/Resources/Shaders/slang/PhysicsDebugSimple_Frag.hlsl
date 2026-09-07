#pragma pack_matrix(column_major)
#ifdef SLANG_HLSL_ENABLE_NVAPI
#include "nvHLSLExtns.h"
#endif

#ifndef __DXC_VERSION_MAJOR
// warning X3557: loop doesn't seem to do anything, forcing loop to unroll
#pragma warning(disable : 3557)
#endif


#line 29 "./HelloTexture_Frag.slang"
struct DrawParams_0
{
    int TextureIndex_0;
    int SamplerIndex_0;
};


#line 45
cbuffer uParams_0 : register(b0)
{
    DrawParams_0 uParams_0;
}

#line 41
Texture2D<float4 >  textures_0[] : register(t0, space2);


#line 40
SamplerState  samplers_0[] : register(s0, space3);


#line 25
struct FSOutput_0
{
    float4 color_0 : SV_Target;
};


#line 20
struct FSInput_0
{
    float2 texCoord_0 : ATTRIBUTE0;
    float3 color_1 : ATTRIBUTE1;
};


#line 48
[shader("pixel")]FSOutput_0 main(FSInput_0 input_0)
{

#line 48
    FSInput_0 _S1 = input_0;
    FSOutput_0 o_0;

#line 54
    if((uParams_0.TextureIndex_0) != int(-1))
    {

#line 55
        o_0.color_0 = textures_0[uParams_0.TextureIndex_0].Sample(samplers_0[uParams_0.SamplerIndex_0], _S1.texCoord_0);

#line 54
    }
    else
    {
        o_0.color_0 = float4(_S1.color_1, 1.0f);

#line 54
    }

#line 61
    return o_0;
}

