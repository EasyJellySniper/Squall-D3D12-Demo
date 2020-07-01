#include "SqForwardInclude.hlsl"
#include "SqLight.hlsl"
#pragma sq_vertex ForwardPassVS
#pragma sq_pixel ForwardPassPS
#pragma sq_keyword _CUTOFF_ON
#pragma sq_keyword _SPEC_GLOSS_MAP
#pragma sq_keyword _EMISSION
#pragma sq_keyword _NORMAL_MAP
#pragma sq_keyword _DETAIL_MAP
#pragma sq_keyword _DETAIL_NORMAL_MAP

struct v2f
{
	float4 vertex : SV_POSITION;
	float4 tex : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
	float3 normal : NORMAL;
#ifdef _NORMAL_MAP
	float3x3 worldToTangent : TEXCOORD2;
#endif
};

v2f ForwardPassVS(VertexInput i)
{
	v2f o = (v2f)0;
	o.vertex = mul(SQ_MATRIX_MVP, float4(i.vertex, 1.0f));
	o.tex.xy = i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw;

	float2 detailUV = lerp(i.uv1, i.uv2, _DetailUV);
	detailUV = detailUV * _DetailAlbedoMap_ST.xy + _DetailAlbedoMap_ST.zw;
	o.tex.zw = detailUV;

	// assume uniform scale, mul normal with world matrix directly
	o.normal = LocalToWorldDir(i.normal);
	o.worldPos = mul(SQ_MATRIX_WORLD, float4(i.vertex, 1.0f)).xyz;

#ifdef _NORMAL_MAP
	o.worldToTangent = CreateTBN(o.normal, i.tangent);
#endif

	return o;
}

float4 ForwardPassPS(v2f i) : SV_Target
{
	// diffuse
	float4 diffuse = GetAlbedo(i.tex.xy, i.tex.zw);
#ifdef _CUTOFF_ON
	clip(diffuse.a - _CutOff);
#endif

	// specular
	float4 specular = GetSpecular(i.tex.xy);
	diffuse.rgb = DiffuseAndSpecularLerp(diffuse.rgb, specular.rgb);

	// normal
	float3 bumpNormal = GetBumpNormal(i.tex.xy, i.tex.zw, i.normal
#ifdef _NORMAL_MAP
		, i.worldToTangent
#endif
		);

	// occlusion 
	float occlusion = GetOcclusion(i.tex.xy);

	// GI

	// BRDF
	diffuse.rgb = LightBRDF(diffuse.rgb, specular.rgb, bumpNormal, i.worldPos);

	// emission
	float3 emission = GetEmission(i.tex.xy);

	float4 output = diffuse * occlusion;
	output.rgb += emission;
	return output;
}