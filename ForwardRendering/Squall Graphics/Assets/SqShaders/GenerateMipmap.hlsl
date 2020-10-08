#define GenerateMipmapRS "RootFlags(0)," \
"DescriptorTable(UAV(u0, numDescriptors=unbounded))," \
"CBV(b0)," \
"RootConstants(num32BitConstants=1, b1)," \
"DescriptorTable( SRV( t0 ,space = 5, numDescriptors = 1) )," \
"DescriptorTable( Sampler( s0 , numDescriptors = unbounded) )"

#include "SqInput.hlsl"
#pragma sq_compute GenerateMipmapCS
#pragma sq_rootsig GenerateMipmapRS

RWTexture2D<float4> _OutMip[] : register(u0);
Texture2D<float4> _SrcMip : register(t0, space5);
int _NumMipLevel : register(b1);

[RootSignature(GenerateMipmapRS)]
[numthreads(8, 8, 1)]
void GenerateMipmapCS(uint3 _globalID : SV_DispatchThreadID, uint3 _groupID : SV_GroupID)
{
	uint w, h;
	_SrcMip.GetDimensions(w, h);

	float2 uv = _globalID.xy + 0.5f;
	uv /= float2(w, h);

	float4 src = _SrcMip.SampleLevel(_SamplerTable[_LinearSampler], uv, 1);
	_OutMip[1][_globalID.xy / 2] = src;
}