#define DepthPrePassRS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
"CBV(b0)," \
"SRV(t0, space=2)," \
"CBV(b2)," \
"DescriptorTable(SRV(t0, numDescriptors=unbounded))," \
"DescriptorTable(Sampler(s0, numDescriptors=unbounded))"

#include "SqInput.hlsl"
#include "SqForwardInclude.hlsl"
#pragma sq_vertex DepthPrePassVS
#pragma sq_pixel DepthPrePassPS
#pragma sq_rootsig DepthPrePassRS
#pragma sq_keyword _CUTOFF_ON
#pragma sq_keyword _NORMAL_MAP
#pragma sq_keyword _DETAIL_NORMAL_MAP
#pragma sq_keyword _SPEC_GLOSS_MAP

struct v2f
{
	float4 vertex : SV_POSITION;
	float4 tex : TEXCOORD0;
	float3 normal : NORMAL;
	float3x3 worldToTangent : TEXCOORD1;
};

[RootSignature(DepthPrePassRS)]
v2f DepthPrePassVS(VertexInput i, uint iid : SV_InstanceID)
{
	v2f o = (v2f)0;

	float4x4 world = _SqInstanceData[iid].world;
	float4 wpos = mul(world, float4(i.vertex, 1.0f));

	o.vertex = mul(SQ_MATRIX_VP, wpos);
	o.tex.xy = i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw;
	o.tex.zw = lerp(i.uv1, i.uv2, _DetailUV);
	o.tex.zw = o.tex.zw * _DetailAlbedoMap_ST.xy + _DetailAlbedoMap_ST.zw;

	// assume uniform scale, mul normal with world matrix directly
	o.normal = LocalToWorldDir(world, i.normal);
	o.worldToTangent = CreateTBN(world, o.normal, i.tangent);

	return o;
}

[RootSignature(DepthPrePassRS)]
float4 DepthPrePassPS(v2f i) : SV_Target
{
#ifdef _CUTOFF_ON
	float alpha = SQ_SAMPLE_TEXTURE(_DiffuseIndex, _SamplerIndex, i.tex.xy).a * _Color.a;
	clip(alpha - _CutOff);
#endif

	// return normal
	float3 bumpNormal = GetBumpNormal(i.tex.xy, i.tex.zw, i.normal, i.worldToTangent);
	return float4(bumpNormal, GetSpecular(i.tex.xy).a);
}