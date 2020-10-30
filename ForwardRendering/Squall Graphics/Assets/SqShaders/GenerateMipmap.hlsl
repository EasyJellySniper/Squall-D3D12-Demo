#define GenerateMipmapRS "RootFlags(0)," \
"DescriptorTable(UAV(u0, numDescriptors=unbounded))," \
"CBV(b0)," \
"RootConstants(num32BitConstants=2, b1)," \
"DescriptorTable( SRV( t0 ,space = 5, numDescriptors = 1) )," \
"DescriptorTable( Sampler( s0 , numDescriptors = unbounded) )"

#include "SqInput.hlsl"
#pragma sq_compute GenerateMipmapCS
#pragma sq_rootsig GenerateMipmapRS

RWTexture2D<float4> _OutMip[] : register(u0);
Texture2D<float4> _SrcMip : register(t0, space5);

cbuffer MipData : register(b1)
{
	int _StartMip;
	int _TotalMip;
};

groupshared float g_R[64];
groupshared float g_G[64];
groupshared float g_B[64];
groupshared float g_A[64];

void CacheColor(uint idx, float4 c)
{
	g_R[idx] = c.r;
	g_G[idx] = c.g;
	g_B[idx] = c.b;
	g_A[idx] = c.a;
}

float4 FetchColor(uint idx)
{
	return float4(g_R[idx], g_G[idx], g_B[idx], g_A[idx]);
}

[RootSignature(GenerateMipmapRS)]
[numthreads(8, 8, 1)]
void GenerateMipmapCS(uint3 _globalID : SV_DispatchThreadID, uint _groupIdx : SV_GroupIndex)
{
	if (_TotalMip == 1 + _StartMip)
	{
		return;
	}

	uint w, h;
	_SrcMip.GetDimensions(w, h);
	w = w >> _StartMip;
	h = h >> _StartMip;

	float2 uv = _globalID.xy + 0.5f;
	uv /= float2(w, h);

	// keep mip 0 color
	float4 src1 = _SrcMip.SampleLevel(_SamplerTable[_LinearWrapSampler], uv, _StartMip);
	CacheColor(_groupIdx, src1);
	GroupMemoryBarrierWithGroupSync();

	// calc mip 1 - for even xy (001001)
	if ((_groupIdx & 0x9) == 0)
	{
		// average 4 points
		float4 src2 = FetchColor(_groupIdx + 1);
		float4 src3 = FetchColor(_groupIdx + 8);
		float4 src4 = FetchColor(_groupIdx + 9);
		src1 = 0.25 * (src1 + src2 + src3 + src4);

		_OutMip[_StartMip + 1][_globalID.xy / 2] = src1;
		CacheColor(_groupIdx, src1);
	}

	if (_TotalMip == 2 + _StartMip)
	{
		return;
	}

	GroupMemoryBarrierWithGroupSync();

	// calc mip 2 - for 4x xy
	if ((_groupIdx & 0x1B) == 0)
	{
		// average 4 points
		float4 src2 = FetchColor(_groupIdx + 2);
		float4 src3 = FetchColor(_groupIdx + 16);
		float4 src4 = FetchColor(_groupIdx + 18);
		src1 = 0.25 * (src1 + src2 + src3 + src4);

		_OutMip[_StartMip + 2][_globalID.xy / 4] = src1;
		CacheColor(_groupIdx, src1);
	}

	if (_TotalMip == 3 + _StartMip)
	{
		return;
	}

	GroupMemoryBarrierWithGroupSync();

	// calc mip 3 - for 8x xy, now thread is 8x8 so only 1 thread enter here
	if (_groupIdx == 0)
	{
		// average 4 points
		float4 src2 = FetchColor(_groupIdx + 4);
		float4 src3 = FetchColor(_groupIdx + 32);
		float4 src4 = FetchColor(_groupIdx + 36);
		src1 = 0.25 * (src1 + src2 + src3 + src4);

		_OutMip[_StartMip + 3][_globalID.xy / 8] = src1;
	}
}