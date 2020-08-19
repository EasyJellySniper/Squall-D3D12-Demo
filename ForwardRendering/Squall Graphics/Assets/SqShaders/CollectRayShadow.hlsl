#define CollectRayShadowRS "CBV(b1)," \
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

[RootSignature(CollectRayShadowRS)]
float4 CollectRayShadowPS(v2f i) : SV_Target
{
    return _TexTable[_RayShadowIndex].Sample(_SamplerTable[_CollectShadowSampler], i.uv).r;
}