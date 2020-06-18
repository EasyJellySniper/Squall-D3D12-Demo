#include "SqForwardInclude.hlsl"
#pragma sq_vertex DepthPrePassVS
#pragma sq_pixel DepthPrePassPS
#pragma sq_keyword _CUTOFF_ON

struct v2f
{
	float4 vertex : SV_POSITION;
	float2 uv1 : TEXCOORD0;
};

v2f DepthPrePassVS(VertexInput i)
{
	v2f o = (v2f)0;
	o.vertex = mul(SQ_MATRIX_MVP, float4(i.vertex, 1.0f));
	o.uv1 = i.uv1;

	return o;
}

void DepthPrePassPS(v2f i)
{
#ifdef _CUTOFF_ON
	float alpha = _TexTable[_DiffuseIndex].Sample(_SamplerTable[_DiffuseSampler], i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw).a;
	clip(alpha - _CutOff);
#endif
}