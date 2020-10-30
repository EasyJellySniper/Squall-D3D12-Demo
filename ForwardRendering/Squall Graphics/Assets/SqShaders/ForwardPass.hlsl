#define ForwardPassRS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
"CBV(b0)," \
"DescriptorTable(SRV(t3, space = 1, numDescriptors=1))," \
"CBV(b1)," \
"SRV(t0, space=2)," \
"CBV(b2)," \
"DescriptorTable(SRV(t0, numDescriptors=unbounded))," \
"DescriptorTable(Sampler(s0, numDescriptors=unbounded))," \
"SRV(t0, space=1)," \
"SRV(t1, space=1)"

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
#pragma sq_keyword _FRESNEL_EFFECT

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
v2f ForwardPassVS(VertexInput i, uint iid : SV_InstanceID)
{
	v2f o = (v2f)0;
	o.tex.xy = i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw;

	float2 detailUV = lerp(i.uv1, i.uv2, _DetailUV);
	detailUV = detailUV * _DetailAlbedoMap_ST.xy + _DetailAlbedoMap_ST.zw;
	o.tex.zw = detailUV;

#ifdef _TRANSPARENT_ON
	float4 wpos = mul(SQ_MATRIX_WORLD, float4(i.vertex, 1.0f));
#else
	float4 wpos = mul(_SqInstanceData[iid].world, float4(i.vertex, 1.0f));
#endif

	o.worldPos = wpos.xyz;
	o.vertex = mul(SQ_MATRIX_VP, wpos);

	// calc normal for transparent only
#ifdef _TRANSPARENT_ON
	// assume uniform scale, mul normal with world matrix directly
	o.normal = LocalToWorldDir(i.normal);
	#ifdef _NORMAL_MAP
		o.worldToTangent = CreateTBN(o.normal, i.tangent);
	#endif
#endif

	return o;
}

[RootSignature(ForwardPassRS)]
float4 ForwardPassPS(v2f i) : SV_Target
{
	float2 screenUV = i.vertex.xy / _ScreenSize.xy;

	// diffuse
	float4 diffuse = GetAlbedo(i.tex.xy, i.tex.zw);
#ifdef _CUTOFF_ON
	clip(diffuse.a - _CutOff);
#endif

	// specular
	float4 specular = GetSpecular(i.tex.xy);
	diffuse.rgb = DiffuseAndSpecularLerp(diffuse.rgb, specular.rgb);

	// calc transparent normal only
#ifdef _TRANSPARENT_ON
	// normal
	float3 bumpNormal = GetBumpNormal(i.tex.xy, i.tex.zw, i.normal
	#ifdef _NORMAL_MAP
			, i.worldToTangent
	#endif
			);
#else
	float3 bumpNormal = SQ_SAMPLE_TEXTURE(_NormalRTIndex, _AnisotropicWrapSampler, screenUV).rgb;
#endif

	// occlusion 
	float occlusion = GetOcclusion(i.tex.xy);

	// GI
#ifdef _TRANSPARENT_ON
	SqGI gi = CalcGI(bumpNormal, screenUV, specular.a, occlusion, true);
#else
	SqGI gi = CalcGI(bumpNormal, screenUV, specular.a, occlusion, false);
#endif

	// shadow
#ifdef _TRANSPARENT_ON
	float shadowAtten = SQ_SAMPLE_TEXTURE(_CollectTransShadowIndex, _AnisotropicWrapSampler, screenUV).r;
#else
	float shadowAtten = SQ_SAMPLE_TEXTURE(_CollectShadowIndex, _AnisotropicWrapSampler, screenUV).r;
#endif

	// BRDF
	diffuse.rgb = LightBRDF(diffuse.rgb, specular.rgb, specular.a, bumpNormal, i.worldPos, i.vertex.xy, shadowAtten, gi);

	// emission
	float3 emission = GetEmission(i.tex.xy);

	float4 output = diffuse;
	output.rgb += emission;
	return output;
}