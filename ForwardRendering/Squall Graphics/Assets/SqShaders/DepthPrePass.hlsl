#include "SqInclude.hlsl"

struct v2f
{
	float4 vertex : SV_POSITION;
	float2 uv1 : TEXCOORD0;
};

cbuffer MaterialConstant : register(b1)
{
	float4 _MainTex_ST;
	float _CutOff;
	int _TexIndex;
	int _SamplerIndex;
	float _Padding;
};

// need /enable_unbounded_descriptor_tables when compiling
Texture2D _TexTable[] : register(t0);
SamplerState _SamplerTable[] : register(s0);

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
	float alpha = _TexTable[_TexIndex].Sample(_SamplerTable[_SamplerIndex], i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw).a;
	clip(alpha - _CutOff);
#endif
}