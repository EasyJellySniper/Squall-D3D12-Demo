#include "SqInclude.hlsl"
#pragma sq_vertex ForwardPassVS
#pragma sq_pixel ForwardPassPS
#pragma sq_keyword _CUTOFF_ON

struct v2f
{
	float4 vertex : SV_POSITION;
	float2 uv1 : TEXCOORD0;
};

cbuffer MaterialConstant : register(b1)
{
	float4 _MainTex_ST;
	float _CutOff;
	int _DiffuseIndex;
	int _DiffuseSampler;
	float _Padding;
};

#pragma sq_srvStart
// need /enable_unbounded_descriptor_tables when compiling
Texture2D _TexTable[] : register(t0);
SamplerState _SamplerTable[] : register(s0);
#pragma sq_srvEnd

v2f ForwardPassVS(VertexInput i)
{
	v2f o = (v2f)0;
	o.vertex = mul(SQ_MATRIX_MVP, float4(i.vertex, 1.0f));
	o.uv1 = i.uv1;

	return o;
}

float4 ForwardPassPS(v2f i) : SV_Target
{
	float2 uvTiled = i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw;
	float4 diffuse = _TexTable[_DiffuseIndex].Sample(_SamplerTable[_DiffuseSampler], uvTiled);

#ifdef _CUTOFF_ON
	clip(diffuse.a - _CutOff);
#endif

	return diffuse;
}