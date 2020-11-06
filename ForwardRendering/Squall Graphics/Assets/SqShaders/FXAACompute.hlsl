#define FXAAComputeRS "RootFlags(0)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"CBV(b0)," \
"RootConstants( num32BitConstants = 4, b1 )," \
"DescriptorTable(SRV(t0, numDescriptors=1))," \
"DescriptorTable(Sampler(s0 , numDescriptors = unbounded))"

#include "SqInput.hlsl"
#pragma sq_compute FXAAComputeCS
#pragma sq_rootsig FXAAComputeRS

RWTexture2D<float4> _OutputTex : register(u0);
Texture2D _InputTex : register(t0);

cbuffer FXAAConstant : register(b1)
{
	float4 targetSize;	// width height 1/width 1/height
};

bool EdgeDetect(int2 uv)
{
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

	if (lumaRange < max(0.0312f, lumaMax * 0.125f))
	{
		return false;
	}

	return true;
}

[RootSignature(FXAAComputeRS)]
[numthreads(8,8,1)]
void FXAAComputeCS(uint3 _globalID : SV_DispatchThreadID)
{
	if (_globalID.x >= targetSize.x || _globalID.y >= targetSize.y)
	{
		return;
	}

	float2 screenUV = (_globalID.xy + 0.5f) * targetSize.zw;

	float4 col = _InputTex[_globalID.xy];
	_OutputTex[_globalID.xy] = lerp(col, float4(1, 0, 0, 0), EdgeDetect(_globalID.xy));
}