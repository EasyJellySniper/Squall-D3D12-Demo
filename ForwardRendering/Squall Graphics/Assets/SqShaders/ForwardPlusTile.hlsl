#define ForwardPlusTileRS "DescriptorTable(UAV(u0, numDescriptors=1))," \
"CBV(b1)," \
"SRV(t1, space=1)," \
"DescriptorTable( SRV( t0 , numDescriptors = unbounded) )," \
"DescriptorTable( Sampler( s0 , numDescriptors = unbounded) )"

#pragma sq_compute ForwardPlusTileCS
#pragma sq_rootsig ForwardPlusTileRS
#include "SqInput.hlsl"

// result
RWByteAddressBuffer _TileResult : register(u0);

groupshared uint minDepthU;
groupshared uint maxDepthU;

// use 32x32 tiles for light culling
[RootSignature(ForwardPlusTileRS)]
[numthreads(32, 32, 1)]
void ForwardPlusTileCS(uint3 _globalID : SV_DispatchThreadID, uint3 _groupID : SV_GroupID, uint _threadIdx : SV_GroupIndex)
{
	float depth = -1;

	if (_globalID.x >= _ScreenSize.x || _globalID.y >= _ScreenSize.y)
	{
		// out of range
		// do nothing here, we need to check depth within a tile
	}
	else
	{
		// fetch depth
		depth = _TexTable[_DepthIndex][_globalID.xy].r;
	}

	// init thread variable and sync
	if (_threadIdx == 0)
	{
		minDepthU = UINT_MAX;
		maxDepthU = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	// find min/max depth
	if (depth != -1.0)
	{
		uint depthU = asuint(depth);

		InterlockedMin(minDepthU, depthU);
		InterlockedMax(maxDepthU, depthU);
	}
	GroupMemoryBarrierWithGroupSync();

	float minDepthF = asfloat(minDepthU);
	float maxDepthF = asfloat(maxDepthU);
}