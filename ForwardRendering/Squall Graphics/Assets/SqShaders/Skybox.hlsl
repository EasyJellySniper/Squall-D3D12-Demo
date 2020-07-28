#define SkyboxRS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
"CBV(b0)," \
"CBV(b1)," \
"DescriptorTable(SRV(t0, numDescriptors=1))," \
"DescriptorTable(Sampler(s0, numDescriptors=1))" \

#include "SqInput.hlsl"
#pragma sq_vertex SkyboxVS
#pragma sq_pixel SkyboxPS
#pragma sq_rootsig SkyboxRS

struct v2f
{
	float4 vertex : SV_POSITION;
	float3 lpos : TEXCOORD0;
};

TextureCube _SkyCube : register(t0);
SamplerState _SkySampler : register(s0);

[RootSignature(SkyboxRS)]
v2f SkyboxVS(VertexInput v)
{
	v2f o = (v2f)0;

	// local pos as sample dir
	o.lpos = v.vertex;

	// always center to camera
	float4 wpos = mul(SQ_MATRIX_WORLD, float4(v.vertex, 1.0f));
	wpos.xyz += _CameraPos;

	o.vertex = mul(SQ_MATRIX_VP, wpos);

	// always on far plane (reverse-z)
	o.vertex.z = 0;

	return o;
}

[RootSignature(SkyboxRS)]
float4 SkyboxPS(v2f i) : SV_Target
{
	return _SkyCube.Sample(_SkySampler, i.lpos) * _SkyIntensity;
}