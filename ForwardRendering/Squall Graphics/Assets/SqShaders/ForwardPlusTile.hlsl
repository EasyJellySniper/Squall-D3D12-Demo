#define ForwardPlusTileRS "RootFlags(0)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"CBV(b0)," \
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
groupshared uint tilePointLightCount;
groupshared uint tilePointLightArray[1024];

// calc view space frustum
void CalcFrustumPlanes(uint tileX, uint tileY, float minZ, float maxZ, out float4 plane[6])
{
	// tile position in screen space
	float x = tileX * TILE_SIZE;
	float y = tileY * TILE_SIZE;
	float rx = x + TILE_SIZE;
	float ry = y + TILE_SIZE;

	// frustum corner (at far plane) - LB RB LT RT
	plane[0] = float4(x, y, maxZ, 1.0f);
	plane[1] = float4(rx, y, maxZ, 1.0f);
	plane[2] = float4(x, ry, maxZ, 1.0f);
	plane[3] = float4(rx, ry, maxZ, 1.0f);

	// near far
	plane[4] = float4(0, 0, minZ, 1.0f);
	plane[5] = float4(0, 0, maxZ, 1.0f);

	[unroll]
	for (uint i = 0; i < 6; i++)
	{
		// convert corners to ndc space
		plane[i].xy *= _ScreenSize.zw;
		plane[i].xy = plane[i].xy * 2.0f - 1.0f;
		plane[i].y = -plane[i].y;

		// convert corners to view position
		plane[i].xyz = DepthToViewPos(plane[i].z, plane[i]);
	}

	// update min/max eye-z
	minZ = plane[4].z;
	maxZ = plane[5].z;

	// dir from camera to corners
	float3 corners[4];
	[unroll]
	for (i = 0; i < 4; i++)
	{
		corners[i].xyz = normalize(plane[i].xyz);
	}

	// plane order: Left, Right, Bottom, Top, Near, Far
	plane[0].xyz = cross(corners[0], corners[2]);
	plane[0].xyz *= lerp(-1, 1, plane[0].x > 0);
	plane[0].w = 0;

	plane[1].xyz = cross(corners[1], corners[3]);
	plane[1].xyz *= lerp(1, -1, plane[1].x > 0);
	plane[1].w = 0;

	plane[2].xyz = cross(corners[0], corners[1]);
	plane[2].xyz *= lerp(-1, 1, plane[2].y > 0);
	plane[2].w = 0;

	plane[3].xyz = cross(corners[2], corners[3]);
	plane[3].xyz *= lerp(1, -1, plane[3].y > 0);
	plane[3].w = 0;

	plane[4].xyz = float3(0, 0, 1);
	plane[4].w = -minZ;

	plane[5].xyz = float3(0, 0, -1);
	plane[5].w = maxZ;
}

// use 32x32 threads per group
[RootSignature(ForwardPlusTileRS)]
[numthreads(TILE_SIZE, TILE_SIZE, 1)]
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
		tilePointLightCount = 0;
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

	// tile data
	uint tileIndex = _groupID.x + _groupID.y * _TileCountX;

	// if thread not out-of-range
	if (_threadIdx < _NumPointLight && asfloat(maxDepthU) > FLOAT_EPSILON)
	{
		float minDepthF = asfloat(minDepthU);
		float maxDepthF = asfloat(maxDepthU);

		// check invalid depth range
		if (minDepthF < FLOAT_EPSILON || maxDepthF - minDepthF < FLOAT_EPSILON)
		{
			minDepthF = maxDepthF - FLOAT_EPSILON;
		}

		float4 plane[6];
		CalcFrustumPlanes(_groupID.x, _groupID.y, maxDepthF, minDepthF, plane);

		// light overlap test
		SqLight light = _SqPointLight[_threadIdx];
		float3 lightPosV = mul(SQ_MATRIX_V, float4(light.world.xyz, 1.0f)).xyz * float3(1, 1, -1);

		bool overlapping = true;
		for (int n = 0; n < 6; n++)
		{
			float d = dot(lightPosV, plane[n].xyz) + plane[n].w;
			if (d < 0)
			{
				overlapping = false;
			}
		}

		if (overlapping)
		{
			uint idx = 0;
			InterlockedAdd(tilePointLightCount, 1, idx);
			tilePointLightArray[idx] = _threadIdx;
		}
	}
	GroupMemoryBarrierWithGroupSync();

	// output by 1st thread
	if (_threadIdx == 0)
	{
		uint tileOffset = tileIndex * (_NumPointLight * 4 + 4);
		_TileResult.Store(tileOffset, tilePointLightCount);

		uint offset = tileOffset + 4;
		for (uint i = 0; i < tilePointLightCount; i++)
		{
			_TileResult.Store(offset, tilePointLightArray[i]);
			offset += 4;
		}
	}
}