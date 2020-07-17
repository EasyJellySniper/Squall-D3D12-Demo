#pragma sq_cbuffer SystemConstant
#pragma sq_srv _SqDirLight
#pragma sq_srv _ShadowMap
#pragma sq_srv _ShadowSampler

#include "SqInput.hlsl"
#pragma sq_vertex CollectShadowVS
#pragma sq_pixel CollectShadowPS

struct v2f
{
	float4 vertex : SV_POSITION;
    float4 screenPos : TEXCOOED0;
};

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

#pragma sq_srvStart
// shadow map, up to 4 cascades, 1st entry is camera's depth
Texture2D _ShadowMap[5] : register(t0);
SamplerComparisonState _ShadowSampler : register(s0);
#pragma sq_srvEnd

v2f CollectShadowVS(uint vid : SV_VertexID)
{
	v2f o = (v2f)0;

    // convert uv to ndc space
    float2 uv = gTexCoords[vid];
    o.vertex = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0, 1);
    o.screenPos = float4(o.vertex.xy, 0, 1);

	return o;
}

// 3x3 pcf 
float ShadowPCF3x3(uint cascade, float4 coord, float texelSize)
{
    // texel
    float dx = texelSize;

    const float2 offsets[9] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
    };

    float shadow = 0.0f;

    [unroll]
    for (uint i = 0; i < 9; i++)
    {
        shadow += _ShadowMap[cascade].SampleCmpLevelZero(_ShadowSampler, coord.xy + offsets[i], coord.z);
    }

    return shadow * 0.1111f;
}

// 4x4 pcf
float ShadowPCF4x4(uint cascade, float4 coord, float texelSize)
{
    // texel 
    float dx = texelSize;
    float hdx = texelSize * 0.5f;

    const float2 offsets[16] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx), float2(2*dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f), float2(2*dx, 0),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx), float2(2*dx, dx),
        float2(-dx,  2*dx), float2(0.0f,  2*dx), float2(dx,  2*dx), float2(2*dx, 2*dx)
    };

    float shadow = 0.0f;

    [unroll]
    for (uint i = 0; i < 16; i++)
    {
        shadow += _ShadowMap[cascade].SampleCmpLevelZero(_ShadowSampler, coord.xy + offsets[i] - float2(hdx, hdx), coord.z);
    }

    return shadow * 0.0625f;
}

// 5x5 pcf
float ShadowPCF5x5(uint cascade, float4 coord, float texelSize)
{
    // texel 
    float dx = texelSize;

    const float2 offsets[25] =
    {
        float2(-2 * dx,  -2 * dx), float2(-dx,  -2 * dx), float2(0,  -2 * dx), float2(dx, -2 * dx), float2(2 * dx, -2 * dx),
        float2(-2 * dx,  -dx), float2(-dx,  -dx), float2(0,  -dx), float2(dx, -dx), float2(2 * dx, -dx),
        float2(-2 * dx,  0), float2(-dx,  0), float2(0,  0), float2(dx, 0), float2(2 * dx, 0),
        float2(-2 * dx,  dx), float2(-dx,  dx), float2(0, dx), float2(dx, dx), float2(2 * dx, dx),
        float2(-2 * dx,  2 * dx), float2(-dx,  2 * dx), float2(0,  2 * dx), float2(dx, 2 * dx), float2(2 * dx, 2 * dx)
    };

    float shadow = 0.0f;

    [unroll]
    for(uint i = 0; i < 25; i++)
    {
        shadow += _ShadowMap[cascade].SampleCmpLevelZero(_ShadowSampler, coord.xy + offsets[i], coord.z);
    }

    return shadow * 0.04f;
}

float4 CollectShadowPS(v2f i) : SV_Target
{
    float depth = _ShadowMap[0].Load(uint3(i.vertex.xy, 0)).r;

    [branch]
    if (depth == 0.0f)
    {
        // early jump out of fragment
        return 1.0f;
    }

    float3 wpos = DepthToWorldPos(depth, i.screenPos);
    float distToCam = length(wpos - _CameraPos);

    SqLight light = _SqDirLight[0];
    uint cascade = light.numCascade;
    float atten = 1;

    for (uint a = 0; a < cascade; a++)
    {
        float4 spos = mul(light.shadowMatrix[a], float4(wpos, 1));
        spos.xyz /= spos.w;                 // ndc space
        spos.xy = spos.xy * 0.5f + 0.5f;    // [0,1]
        spos.y = 1 - spos.y;                // need to flip shadow map
        spos.z += light.world.w;            // bias to depth

        // test invalid shadow
        float shadowTest = _ShadowMap[a + 1].Load(uint3(spos.xy * light.shadowSize, 0)).r;
        [branch]
        if (shadowTest == 0.0f)
            continue;

        float texelSize = 1.0f / light.shadowSize;
        float shadow = 1;

        // calc weight
        float casDistPrev = light.cascadeDist[a - 1];
        float weight = lerp(1, saturate(distToCam - casDistPrev), a > 0);
        
        [branch]
        if (_PCFIndex == 0)
            shadow = ShadowPCF3x3(a + 1, spos, texelSize);
        else if(_PCFIndex == 1)
            shadow = ShadowPCF4x4(a + 1, spos, texelSize);
        else if(_PCFIndex == 2)
            shadow = ShadowPCF5x5(a + 1, spos, texelSize);

        shadow = lerp(1, lerp(1, shadow, weight), light.color.a);
        atten = min(shadow, atten);
    }

    return atten;
}