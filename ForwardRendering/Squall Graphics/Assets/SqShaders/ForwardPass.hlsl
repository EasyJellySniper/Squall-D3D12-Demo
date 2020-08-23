#define ForwardPassRS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
"CBV(b0)," \
"CBV(b1)," \
"CBV(b2)," \
"CBV(b3)," \
"DescriptorTable(SRV(t0, numDescriptors=unbounded))," \
"DescriptorTable(Sampler(s0, numDescriptors=unbounded))," \
"SRV(t0, space=1)"

#include "SqForwardInclude.hlsl"
#include "SqLight.hlsl"

#pragma sq_vertex ForwardPassVS
#pragma sq_pixel ForwardPassPS
#pragma sq_rootsig ForwardPassRS

#pragma sq_keyword _CUTOFF_ON
#pragma sq_keyword _SPEC_GLOSS_MAP
#pragma sq_keyword _EMISSION
#pragma sq_keyword _NORMAL_MAP
#pragma sq_keyword _DETAIL_MAP
#pragma sq_keyword _DETAIL_NORMAL_MAP
#pragma sq_keyword _TRANSPARENT_ON

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

[RootSignature(ForwardPassRS)]
v2f ForwardPassVS(VertexInput i)
{
	v2f o = (v2f)0;
	o.tex.xy = i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw;

	float2 detailUV = lerp(i.uv1, i.uv2, _DetailUV);
	detailUV = detailUV * _DetailAlbedoMap_ST.xy + _DetailAlbedoMap_ST.zw;
	o.tex.zw = detailUV;

	// assume uniform scale, mul normal with world matrix directly
	o.normal = LocalToWorldDir(i.normal);

	float4 wpos = mul(SQ_MATRIX_WORLD, float4(i.vertex, 1.0f));
	o.worldPos = wpos.xyz;
	o.vertex = mul(SQ_MATRIX_VP, wpos);

#ifdef _NORMAL_MAP
	o.worldToTangent = CreateTBN(o.normal, i.tangent);
#endif

	return o;
}

[RootSignature(ForwardPassRS)]
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
	SqGI gi = CalcGI(bumpNormal, occlusion);

	// BRDF
	float atten = _TexTable[_CollectShadowIndex].Sample(_SamplerTable[_CollectShadowSampler], i.vertex.xy / _ScreenSize).r;

	diffuse.rgb = LightBRDF(diffuse.rgb, specular.rgb, specular.a, bumpNormal, i.worldPos, atten, gi);

	// emission
	float3 emission = GetEmission(i.tex.xy);

	float4 output = diffuse;
	output.rgb += emission;
	return output;
}