#include "SqInput.hlsl"
#pragma sq_vertex ShadowPassVS
#pragma sq_pixel ShadowPassPS
#pragma sq_keyword _SHADOW_CUTOFF_ON

struct v2f
{
	float4 vertex : SV_POSITION;
	float2 uv1 : TEXCOORD0;
};

v2f ShadowPassVS(VertexInput i)
{
	v2f o = (v2f)0;

	float4 wpos = mul(SQ_MATRIX_WORLD, float4(i.vertex, 1.0f));

	// need to offset shadow map handle on CPU side
	o.vertex = mul(SQ_MATRIX_SHADOW, wpos);
	o.uv1 = i.uv1;

	return o;
}

void ShadowPassPS(v2f i)
{
#ifdef _SHADOW_CUTOFF_ON
	float alpha = _TexTable[_DiffuseIndex].Sample(_SamplerTable[_SamplerIndex], i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw).a;
	clip(alpha - _CutOff);
#endif
}