#include "SqForwardInclude.hlsl"
#pragma sq_vertex ForwardPassVS
#pragma sq_pixel ForwardPassPS
#pragma sq_keyword _CUTOFF_ON
#pragma sq_keyword _SPEC_GLOSS_MAP

struct v2f
{
	float4 vertex : SV_POSITION;
	float2 uv1 : TEXCOORD0;
};

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
	float4 diffuse = _TexTable[_DiffuseIndex].Sample(_SamplerTable[_DiffuseSampler], uvTiled) * _Color;
	float3 specular = GetSpecular(uvTiled);

	float4 output = 0;
	output.rgb = DiffuseAndSpecularLerp(diffuse, specular);
	output.a = diffuse.a;

#ifdef _CUTOFF_ON
	clip(output.a - _CutOff);
#endif

	return output;
}