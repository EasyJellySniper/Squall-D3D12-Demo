#define FXAAComputeRS "RootFlags(0)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"CBV(b0)," \
"RootConstants( num32BitConstants = 6, b1 )," \
"DescriptorTable(SRV(t0, numDescriptors=1))," \
"DescriptorTable(Sampler(s0 , numDescriptors = unbounded))"

#include "SqInput.hlsl"
#pragma sq_compute FXAAComputeCS
#pragma sq_rootsig FXAAComputeRS
#define ITERATIONS 12
#define SUBPIXEL_QUALITY 0.75

static const float _Quality[12] = { 1,1,1,1,1,1.5f,2.0f,2.0f,2.0f,2.0f,4.0f,8.0f };
RWTexture2D<float4> _OutputTex : register(u0);
Texture2D _InputTex : register(t0);
#define SAMPLE_INPUT(x) _InputTex.SampleLevel(_SqSamplerTable[_LinearClampSampler], x, 0)

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
	float stepLength;
	float lumaCenter;
	float subPixelOffsetFinal;
};

EdgeData EdgeDetect(float2 uv)
{
	EdgeData ed = (EdgeData)0;

	// center luma
	float lumaCenter = RgbToLuma(SAMPLE_INPUT(uv).rgb);
	ed.lumaCenter = lumaCenter;

	// 4 neighbor pixel
	float lumaDown = RgbToLuma(SAMPLE_INPUT(uv + float2(0, -_TargetSize.w)).rgb);
	float lumaUp = RgbToLuma(SAMPLE_INPUT(uv + float2(0, _TargetSize.w)).rgb);
	float lumaLeft = RgbToLuma(SAMPLE_INPUT(uv + float2(-_TargetSize.z, 0)).rgb);
	float lumaRight = RgbToLuma(SAMPLE_INPUT(uv + float2(_TargetSize.z, 0)).rgb);

	// find min/max luma
	float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
	float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));

	// compute delta
	float lumaRange = lumaMax - lumaMin;
	ed.isEdge = !(lumaRange < max(_EdgeThresholdMin, lumaMax* _EdgeThresholdMax));

	// corner luma
	float lumaDownLeft = RgbToLuma(SAMPLE_INPUT(uv + float2(-_TargetSize.z, -_TargetSize.w)).rgb);
	float lumaUpRight = RgbToLuma(SAMPLE_INPUT(uv + float2(_TargetSize.z, _TargetSize.w)).rgb);
	float lumaUpLeft = RgbToLuma(SAMPLE_INPUT(uv + float2(-_TargetSize.z, _TargetSize.w)).rgb);
	float lumaDownRight = RgbToLuma(SAMPLE_INPUT(uv + float2(_TargetSize.z, -_TargetSize.w)).rgb);

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
	ed.stepLength = ed.isHorizontal ? _TargetSize.w : _TargetSize.z;
	float lumaLocalAverage = 0.0;

	if (is1Steepest) 
	{
		// Switch the direction
		ed.stepLength = -ed.stepLength;
		ed.lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
	}
	else 
	{
		ed.lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
	}

	// --------------------------------------------------------- Sub-pixel shifting --------------------------------------------------------- // 

	// Full weighted average of the luma over the 3x3 neighborhood.
	float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);

	// Ratio of the delta between the global average and the center luma, over the luma range in the 3x3 neighborhood.
	float subPixelOffset1 = clamp(abs(lumaAverage - lumaCenter) / lumaRange, 0.0, 1.0);
	float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;

	// Compute a sub-pixel offset based on this delta.
	ed.subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;

	return ed;
}

// FXAA Steps:
// 1. Find edge by lumination
// 2. Find edge orientation
// 3. Choose edge orienation (shift to edge uv)
// 4. Explore along edge point to both direction (iteration until no edges)
// 5. Check luma of end is correct variation as center luma
// 6. Subpixel anti aliasing for thin pixels
// 7. final read

[RootSignature(FXAAComputeRS)]
[numthreads(8,8,1)]
void FXAAComputeCS(uint3 _globalID : SV_DispatchThreadID)
{
	if (_globalID.x >= _TargetSize.x || _globalID.y >= _TargetSize.y)
	{
		return;
	}

	float2 uv = (_globalID.xy + 0.5f) * _TargetSize.zw;
	float2 originUV = uv;
	EdgeData ed = EdgeDetect(uv);

	if (ed.isEdge > 0)
	{
		if (ed.isHorizontal)
		{
			uv.y += 0.5f * ed.stepLength;
		}
		else
		{
			uv.x += 0.5f * ed.stepLength;
		}

		float2 offset = ed.isHorizontal ? float2(_TargetSize.z, 0.0f) : float2(0.0f, _TargetSize.w);

		// explore edge
		float2 uv1 = uv - offset;
		float2 uv2 = uv + offset;

		// Read the lumas at both current extremities of the exploration segment, and compute the delta wrt to the local average luma.
		float lumaEnd1 = RgbToLuma(SAMPLE_INPUT(uv1).rgb);
		float lumaEnd2 = RgbToLuma(SAMPLE_INPUT(uv2).rgb);
		lumaEnd1 -= ed.lumaLocalAverage;
		lumaEnd2 -= ed.lumaLocalAverage;

		// If the luma deltas at the current extremities are larger than the local gradient, we have reached the side of the edge.
		bool reached1 = abs(lumaEnd1) >= ed.gradientScaled;
		bool reached2 = abs(lumaEnd2) >= ed.gradientScaled;
		bool reachedBoth = reached1 && reached2;

		// If the side is not reached, we continue to explore in this direction.
		if (!reached1)
		{
			uv1 -= offset;
		}

		if (!reached2) 
		{
			uv2 += offset;
		}

		if (!reachedBoth) 
		{
			for (int i = 2; i < ITERATIONS; i++) 
			{
				if (!reached1) 
				{
					lumaEnd1 = RgbToLuma(SAMPLE_INPUT(uv1).rgb);
					lumaEnd1 = lumaEnd1 - ed.lumaLocalAverage;
				}

				if (!reached2) 
				{
					lumaEnd2 = RgbToLuma(SAMPLE_INPUT(uv2).rgb);
					lumaEnd2 = lumaEnd2 - ed.lumaLocalAverage;
				}

				// If the luma deltas at the current extremities is larger than the local gradient, we have reached the side of the edge.
				reached1 = abs(lumaEnd1) >= ed.gradientScaled;
				reached2 = abs(lumaEnd2) >= ed.gradientScaled;
				reachedBoth = reached1 && reached2;

				// If the side is not reached, we continue to explore in this direction, with a variable quality.
				if (!reached1) 
				{
					uv1 -= offset * _Quality[i];
				}

				if (!reached2) 
				{
					uv2 += offset * _Quality[i];
				}

				if (reachedBoth) 
				{
					break;
				}
			}
		}

		// Compute the distances to each extremity of the edge.
		float distance1 = ed.isHorizontal ? (originUV.x - uv1.x) : (originUV.y - uv1.y);
		float distance2 = ed.isHorizontal ? (uv2.x - originUV.x) : (uv2.y - originUV.y);

		// which edge is closer
		bool isDirection1 = distance1 < distance2;
		float distanceFinal = min(distance1, distance2);
		float edgeThickness = (distance1 + distance2);
		float pixelOffset = -distanceFinal / edgeThickness + 0.5f;

		// extra center luma checking
		bool isLumaCenterSmaller = ed.lumaCenter < ed.lumaLocalAverage;
		bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0f) != isLumaCenterSmaller;
		float finalOffset = correctVariation ? pixelOffset : 0.0f;
		finalOffset = max(finalOffset, ed.subPixelOffsetFinal);

		// final uv read
		float2 finalUV = originUV;
		if (ed.isHorizontal) 
		{
			finalUV.y += finalOffset * ed.stepLength;
		}
		else 
		{
			finalUV.x += finalOffset * ed.stepLength;
		}

		_OutputTex[_globalID.xy] = SAMPLE_INPUT(finalUV);
	}
}