#pragma sq_cbuffer SystemConstant
#pragma sq_srv _SqDirLight
#pragma sq_srv _DepthMap
#pragma sq_srv _ShadowMap

#include "SqInput.hlsl"
#pragma sq_vertex CollectShadowVS
#pragma sq_pixel CollectShadowPS

struct v2f
{
	float4 vertex : SV_POSITION;
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
Texture2D _DepthMap : register(t0);
// shadow map, up to 4 cascades
Texture2D _ShadowMap[4] : register(t1);
#pragma sq_srvEnd

v2f CollectShadowVS(uint vid : SV_VertexID)
{
	v2f o = (v2f)0;

    // convert uv to ndc space
    float2 uv = gTexCoords[vid];
    o.vertex = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0, 1);

	return o;
}

float4 CollectShadowPS(v2f i) : SV_Target
{
    return 1.0f;
}