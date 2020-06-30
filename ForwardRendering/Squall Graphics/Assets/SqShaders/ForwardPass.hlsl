#include "SqForwardInclude.hlsl"
#pragma sq_vertex ForwardPassVS
#pragma sq_pixel ForwardPassPS
#pragma sq_keyword _CUTOFF_ON
#pragma sq_keyword _SPEC_GLOSS_MAP
#pragma sq_keyword _EMISSION

struct v2f
{
	float4 vertex : SV_POSITION;
	float2 uv1 : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
	float3 normal : NORMAL;
};

v2f ForwardPassVS(VertexInput i)
{
	v2f o = (v2f)0;
	o.vertex = mul(SQ_MATRIX_MVP, float4(i.vertex, 1.0f));
	o.uv1 = i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw;

	// assume uniform scale, mul normal with world matrix directly
	o.normal = mul((float3x3)SQ_MATRIX_WORLD, i.normal);
	o.worldPos = mul(SQ_MATRIX_WORLD, float4(i.vertex, 1.0f)).xyz;

	return o;
}

float4 ForwardPassPS(v2f i) : SV_Target
{
	// diffuse
	float4 diffuse = _TexTable[_DiffuseIndex].Sample(_SamplerTable[_DiffuseSampler], i.uv1) * _Color;
#ifdef _CUTOFF_ON
	clip(diffuse.a - _CutOff);
#endif

	// specular
	float4 specular = GetSpecular(i.uv1);
	diffuse.rgb = DiffuseAndSpecularLerp(diffuse.rgb, specular.rgb);

	// occlusion 
	float occlusion = GetOcclusion(i.uv1);

	// GI

	// BRDF
	diffuse.rgb = LightBRDF(diffuse.rgb, specular.rgb, i.normal, i.worldPos);

	// emission
	float3 emission = GetEmission(i.uv1);

	float4 output = diffuse * occlusion;
	output.rgb += emission;
	return output;
}