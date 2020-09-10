#define CollectRayShadowRS "CBV(b0)," \
"DescriptorTable(SRV(t0, numDescriptors=unbounded))," \
"DescriptorTable(Sampler(s0, numDescriptors=unbounded))"

#include "SqInput.hlsl"
#pragma sq_vertex CollectRayShadowVS
#pragma sq_pixel CollectRayShadowPS
#pragma sq_rootsig CollectRayShadowRS

struct v2f
{
	float4 vertex : SV_POSITION;
    float4 screenPos : TEXCOOED0;
    float2 uv : TEXCOORD1;
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

v2f CollectRayShadowVS(uint vid : SV_VertexID)
{
	v2f o = (v2f)0;

    // convert uv to ndc space
    float2 uv = gTexCoords[vid];
    o.vertex = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0, 1);
    o.screenPos = float4(o.vertex.xy, 0, 1);
    o.uv = uv;

	return o;
}

float PenumbraFilter(float2 uv, int innerLoop)
{
    uint2 d;
    _TexTable[_RayShadowIndex].GetDimensions(d.x, d.y);

    float blockCount = 0;
    float avgBlockDepth = 0;
    float avgReceiverDepth = 0;
    float lightSize = 0;

    float2 texelSize = 1.0f / d;
    for (int i = -innerLoop; i <= innerLoop; i++)
    {
        for (int j = -innerLoop; j <= innerLoop; j++)
        {
            // x for blocked, y for dist to blocker
            float4 shadowData = _TexTable[_RayShadowIndex].Sample(_SamplerTable[_CollectShadowSampler], uv + texelSize * float2(i, j)).rgba;

            [branch]
            if (shadowData.x < 1)
            {
                avgBlockDepth += shadowData.y;      // blocker to light distance
                avgReceiverDepth += shadowData.z;   // receiver to light distance
                lightSize += shadowData.w;          // light size
                blockCount++;
            }
        }
    }

    [branch]
    if (blockCount > 0)
    {
        avgBlockDepth /= blockCount;
        avgReceiverDepth /= blockCount;
        lightSize /= blockCount;

        // penumbra formula: (d_receiver - d_blocker) * light_size / d_blocker
        float penumbra = (avgReceiverDepth - avgBlockDepth) * lightSize / avgBlockDepth;
        penumbra = pow(penumbra, 0.5f);
        return penumbra;
    }

    return 0.0f;
}

float BlurFilter(float2 uv, int innerLoop, float penumbra)
{
    float center = _TexTable[_RayShadowIndex].Sample(_SamplerTable[_CollectShadowSampler], uv).r;
    [branch]
    if (penumbra < FLOAT_EPSILON)
    {
        // early out if doesn't need blur
        return center;
    }

    uint2 d;
    _TexTable[_RayShadowIndex].GetDimensions(d.x, d.y);

    float atten = 0;
    float2 texelSize = 1.0f / d;
    int count = 0;

    [loop]
    for (int i = -innerLoop; i <= innerLoop; i++)
    {
        [loop]
        for (int j = -innerLoop; j <= innerLoop; j++)
        {
            float w = (innerLoop - abs(i) + 1) * (innerLoop - abs(j) + 1);
            atten += _TexTable[_RayShadowIndex].Sample(_SamplerTable[_CollectShadowSampler], uv + float2(i, j) * texelSize * penumbra).r * w;
            count += w;
        }
    }

    return atten / count;
}

[RootSignature(CollectRayShadowRS)]
float4 CollectRayShadowPS(v2f i) : SV_Target
{
    float depth = _TexTable[_TransDepthIndex].Sample(_SamplerTable[_CollectShadowSampler], i.uv).r;
    float3 wpos = DepthToWorldPos(depth, i.screenPos);
    float distRatio = 1 - saturate(abs(wpos.z - _CameraPos.z) * 0.05f);

    // reduce penumbra according to distance
    // prevent far object blurring too much

    float penumbra = PenumbraFilter(i.uv, _PCFIndex);
    return BlurFilter(i.uv, _PCFIndex, penumbra * distRatio);
}