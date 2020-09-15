#define ForwardPlusTileRS "RootFlags(0)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"DescriptorTable(UAV(u1, numDescriptors=1))," \
"CBV(b0)," \
"SRV(t1, space=1)," \
"DescriptorTable( SRV( t0 , numDescriptors = unbounded) )," \
"DescriptorTable( Sampler( s0 , numDescriptors = unbounded) )"

#pragma sq_compute ForwardPlusTileCS
#pragma sq_rootsig ForwardPlusTileRS
#include "SqInput.hlsl"

// result
RWByteAddressBuffer _PointLightTile : register(u0);
RWByteAddressBuffer _PointLightTransTile : register(u0);

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
	float rx = (tileX + 1) * TILE_SIZE;
	float ry = (tileY + 1) * TILE_SIZE;

	// frustum corner (at far plane) - LB RB LT RT
	plane[0] = float4(x, y, 0.0f, 1.0f);
	plane[1] = float4(rx, y, 0.0f, 1.0f);
	plane[2] = float4(x, ry, 0.0f, 1.0f);
	plane[3] = float4(rx, ry, 0.0f, 1.0f);

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
		corners[i].xyz = plane[i].xyz;
	}

	// plane order: Left, Right, Bottom, Top, Near, Far
	// note the cross order: top-to-bottom left-to-right
	plane[0].xyz = normalize(cross(corners[2], corners[0]));
	plane[0].w = 0;

	plane[1].xyz = -normalize(cross(corners[3], corners[1]));	// flip so right plane point inside frustum
	plane[1].w = 0;

	plane[2].xyz = normalize(cross(corners[0], corners[1]));
	plane[2].w = 0;

	plane[3].xyz = -normalize(cross(corners[2], corners[3]));	// flip so top plane point inside frustum
	plane[3].w = 0;

	plane[4].xyz = float3(0, 0, 1);
	plane[4].w = -minZ;

	plane[5].xyz = float3(0, 0, -1);
	plane[5].w = maxZ;
}

// sphere inside frustum
bool SphereInsideFrustum(float4 sphere, float4 plane[6])
{
	bool result = true;

	for (int n = 0; n < 6; n++)
	{
		float d = dot(sphere.xyz, plane[n].xyz) + plane[n].w;
		if (d < -sphere.w)
		{
			result = false;
		}
	}

	return result;
}

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
void CollectLight(uint2 _groupID, uint _threadIdx, uint minDepthU, uint maxDepthU)
{
	// if thread not out-of-range & mapdepth is valid
	if (_threadIdx < _NumPointLight)
	{
		float minDepthF = asfloat(minDepthU);
		float maxDepthF = asfloat(maxDepthU);

		// prevent invalid depth range
		if (maxDepthF - minDepthF < FLOAT_EPSILON)
		{
			maxDepthF = minDepthF + FLOAT_EPSILON;
		}

		float4 plane[6];
		CalcFrustumPlanes(_groupID.x, _groupID.y, maxDepthF, minDepthF, plane);

		// light overlap test
		SqLight light = _SqPointLight[_threadIdx];
		float3 lightPosV = mul(SQ_MATRIX_V, float4(light.world.xyz, 1.0f)).xyz * float3(1, 1, -1);
		float4 sphere = float4(lightPosV, light.range);

		bool overlapping = SphereInsideFrustum(sphere, plane);
		if (overlapping)
		{
			uint idx = 0;
			InterlockedAdd(tilePointLightCount, 1, idx);
			tilePointLightArray[idx] = _threadIdx;
		}

		// transparent test, set near plane to 0
		plane[4].w = 0.0f;
		overlapping = SphereInsideFrustum(sphere, plane);
		if (overlapping)
		{
			uint idx = 0;
			InterlockedAdd(tilePointLightTransCount, 1, idx);
			tilePointLightTransArray[idx] = _threadIdx;
		}
	}
	GroupMemoryBarrierWithGroupSync();
}

=======
>>>>>>> parent of c9c2998... Store transparent point tile
=======
>>>>>>> parent of c9c2998... Store transparent point tile
=======
>>>>>>> parent of c9c2998... Store transparent point tile
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
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
	CollectLight(_groupID.xy, _threadIdx, minDepthU, maxDepthU);
=======
>>>>>>> parent of c9c2998... Store transparent point tile
=======
>>>>>>> parent of c9c2998... Store transparent point tile
=======
>>>>>>> parent of c9c2998... Store transparent point tile

	// if thread not out-of-range & mapdepth is valid
	if (_threadIdx < _NumPointLight && asfloat(maxDepthU) > FLOAT_EPSILON)
	{
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
		uint tileOffset = GetPointLightOffset(tileIndex);
=======
		float minDepthF = asfloat(minDepthU);
		float maxDepthF = asfloat(maxDepthU);
>>>>>>> parent of c9c2998... Store transparent point tile
=======
		float minDepthF = asfloat(minDepthU);
		float maxDepthF = asfloat(maxDepthU);
>>>>>>> parent of c9c2998... Store transparent point tile
=======
		float minDepthF = asfloat(minDepthU);
		float maxDepthF = asfloat(maxDepthU);
>>>>>>> parent of c9c2998... Store transparent point tile

		// prevent invalid depth range
		if (maxDepthF - minDepthF < FLOAT_EPSILON)
		{
			maxDepthF = minDepthF + FLOAT_EPSILON;
		}

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
		// store transparent
		tileOffset = GetPointLightOffset(tileIndex);
		_PointLightTransTile.Store(tileOffset, tilePointLightTransCount);
		tileOffset += 4;
		for (i = 0; i < tilePointLightTransCount; i++)
=======
=======
>>>>>>> parent of c9c2998... Store transparent point tile
=======
>>>>>>> parent of c9c2998... Store transparent point tile
		float4 plane[6];
		CalcFrustumPlanes(_groupID.x, _groupID.y, maxDepthF, minDepthF, plane);

		// light overlap test
		SqLight light = _SqPointLight[_threadIdx];
		float3 lightPosV = mul(SQ_MATRIX_V, float4(light.world.xyz, 1.0f)).xyz * float3(1, 1, -1);

		bool overlapping = SphereInsideFrustum(float4(lightPosV, light.range), plane);
		if (overlapping)
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> parent of c9c2998... Store transparent point tile
=======
>>>>>>> parent of c9c2998... Store transparent point tile
=======
>>>>>>> parent of c9c2998... Store transparent point tile
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
		_PointLightTile.Store(tileOffset, tilePointLightCount);

		uint offset = tileOffset + 4;
		for (uint i = 0; i < tilePointLightCount; i++)
		{
			_PointLightTile.Store(offset, tilePointLightArray[i]);
			offset += 4;
		}
	}
}