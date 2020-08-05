#define DepthPrePassRS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
"CBV(b0)," \
"CBV(b1)," \
"CBV(b3)," \
"DescriptorTable(SRV(t0, numDescriptors=unbounded))," \
"DescriptorTable(Sampler(s0, numDescriptors=unbounded))"

#include "SqInput.hlsl"
#pragma sq_vertex DepthPrePassVS
#pragma sq_pixel DepthPrePassPS
#pragma sq_rootsig DepthPrePassRS
#pragma sq_keyword _CUTOFF_ON

struct v2f
{
	float4 vertex : SV_POSITION;
	float2 uv1 : TEXCOORD0;
};

[RootSignature(DepthPrePassRS)]
v2f DepthPrePassVS(VertexInput i)
{
	v2f o = (v2f)0;

	float4 wpos = mul(SQ_MATRIX_WORLD, float4(i.vertex, 1.0f));
	o.vertex = mul(SQ_MATRIX_VP, wpos);
	o.uv1 = i.uv1;

	return o;
}

[RootSignature(DepthPrePassRS)]
void DepthPrePassPS(v2f i)
{
#ifdef _CUTOFF_ON
	float alpha = _TexTable[_DiffuseIndex].Sample(_SamplerTable[_SamplerIndex], i.uv1 * _MainTex_ST.xy + _MainTex_ST.zw).a;
	clip(alpha - _CutOff);
#endif
}