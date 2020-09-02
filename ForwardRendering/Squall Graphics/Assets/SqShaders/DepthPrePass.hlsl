#define DepthPrePassRS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
"CBV(b0)," \
"CBV(b1)," \
"CBV(b3)," \
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

struct v2f
{
	float4 vertex : SV_POSITION;
	float4 tex : TEXCOORD0;
	float3 normal : NORMAL;
	float3x3 worldToTangent : TEXCOORD1;
};

[RootSignature(DepthPrePassRS)]
v2f DepthPrePassVS(VertexInput i)
{
	v2f o = (v2f)0;

	float4 wpos = mul(SQ_MATRIX_WORLD, float4(i.vertex, 1.0f));
	o.vertex = mul(SQ_MATRIX_VP, wpos);
	o.tex.xy = i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw;

	float2 detailUV = lerp(i.uv1, i.uv2, _DetailUV);
	detailUV = detailUV * _DetailAlbedoMap_ST.xy + _DetailAlbedoMap_ST.zw;
	o.tex.zw = detailUV;

	// assume uniform scale, mul normal with world matrix directly
	o.normal = LocalToWorldDir(i.normal);
	o.worldToTangent = CreateTBN(o.normal, i.tangent);

	return o;
}

[RootSignature(DepthPrePassRS)]
float4 DepthPrePassPS(v2f i) : SV_Target
{
#ifdef _CUTOFF_ON
	float alpha = _TexTable[_DiffuseIndex].Sample(_SamplerTable[_SamplerIndex], i.tex.xy).a;
	clip(alpha - _CutOff);
#endif

	// return normal
	float3 bumpNormal = GetBumpNormal(i.tex.xy, i.tex.zw, i.normal, i.worldToTangent);
	return float4(bumpNormal * 0.5f + 0.5f, 1.0f);
}