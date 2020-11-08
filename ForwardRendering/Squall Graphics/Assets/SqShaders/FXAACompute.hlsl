#define FXAAComputeRS "RootFlags(0)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"CBV(b0)," \
"RootConstants( num32BitConstants = 6, b1 )," \
"DescriptorTable(SRV(t0, numDescriptors=1))," \
"DescriptorTable(Sampler(s0 , numDescriptors = unbounded))"

#include "SqInput.hlsl"
#pragma sq_compute FXAAComputeCS
#pragma sq_rootsig FXAAComputeRS

RWTexture2D<float4> _OutputTex : register(u0);
Texture2D _InputTex : register(t0);

cbuffer FXAAConstant : register(b1)
{
	float4 _TargetSize;	// width height 1/width 1/height
	float _EdgeThresholdMin;
	float _EdgeThresholdMax;
};

struct EdgeData
{
	bool isEdge;
	bool isHorizontal;
	float gradientScaled;
	float lumaLocalAverage;
};

EdgeData EdgeDetect(int2 uv)
{
	EdgeData ed = (EdgeData)0;

	// center luma
	float lumaCenter = RgbToLuma(_InputTex[uv].rgb);

	// 4 neighbor pixel
	float lumaDown = RgbToLuma(_InputTex[uv + int2(0, -1)].rgb);
	float lumaUp = RgbToLuma(_InputTex[uv + int2(0, 1)].rgb);
	float lumaLeft = RgbToLuma(_InputTex[uv + int2(-1, 0)].rgb);
	float lumaRight = RgbToLuma(_InputTex[uv + int2(1, 0)].rgb);

	// find min/max luma
	float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
	float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));

	// compute delta
	float lumaRange = lumaMax - lumaMin;
	ed.isEdge = !(lumaRange < max(_EdgeThresholdMin, lumaMax* _EdgeThresholdMax));

	// corner luma
	float lumaDownLeft = RgbToLuma(_InputTex[uv + int2(-1, -1)].rgb);
	float lumaUpRight = RgbToLuma(_InputTex[uv + int2(1, 1)].rgb);
	float lumaUpLeft = RgbToLuma(_InputTex[uv + int2(-1, 1)].rgb);
	float lumaDownRight = RgbToLuma(_InputTex[uv + int2(1, -1)].rgb);

	// combine edge luma for H/V calc
	float lumaDownUp = lumaDown + lumaUp;
	float lumaLeftRight = lumaLeft + lumaRight;

	// combine corner luma for H/V calc
	float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
	float lumaDownCorners = lumaDownLeft + lumaDownRight;
	float lumaRightCorners = lumaDownRight + lumaUpRight;
	float lumaUpCorners = lumaUpRight + lumaUpLeft;

	//horizontal: | (upleft - left) - (left - downleft) | +2 * | (up - center) - (center - down) | +| (upright - right) - (right - downright) |
	//vertical : | (upright - up) - (up - upleft) | +2 * | (right - center) - (center - left) | +| (downright - down) - (down - downleft) |
	float edgeHorizontal = abs(-2.0 * lumaLeft + lumaLeftCorners) + abs(-2.0 * lumaCenter + lumaDownUp) * 2.0 + abs(-2.0 * lumaRight + lumaRightCorners);
	float edgeVertical = abs(-2.0 * lumaUp + lumaUpCorners) + abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0 + abs(-2.0 * lumaDown + lumaDownCorners);
	ed.isHorizontal = (edgeHorizontal >= edgeVertical);

	// --------------------------------------------------------- Edge Oritention --------------------------------------------------------- // 

	// Select opposite edge.
	float luma1 = ed.isHorizontal ? lumaDown : lumaLeft;
	float luma2 = ed.isHorizontal ? lumaUp : lumaRight;

	// gradient
	float gradient1 = luma1 - lumaCenter;
	float gradient2 = luma2 - lumaCenter;

	// Which direction is the steepest ?
	bool is1Steepest = abs(gradient1) >= abs(gradient2);
	ed.gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

	// Average luma in the correct direction (opposite too).
	float stepLength = ed.isHorizontal ? _TargetSize.w : _TargetSize.z;
	float lumaLocalAverage = 0.0;

	if (is1Steepest) 
	{
		// Switch the direction
		stepLength = -stepLength;
		ed.lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
	}
	else 
	{
		ed.lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
	}

	return ed;
}

// FXAA Steps:
// 1. Find edge by lumination
// 2. Find edge orientation
// 3. Choose edge orienation
[RootSignature(FXAAComputeRS)]
[numthreads(8,8,1)]
void FXAAComputeCS(uint3 _globalID : SV_DispatchThreadID)
{
	if (_globalID.x >= _TargetSize.x || _globalID.y >= _TargetSize.y)
	{
		return;
	}

	EdgeData ed = EdgeDetect(_globalID.xy);
	if (ed.isEdge > 0)
	{
		_OutputTex[_globalID.xy] = float4(1, 0, 0, 0);
	}
}