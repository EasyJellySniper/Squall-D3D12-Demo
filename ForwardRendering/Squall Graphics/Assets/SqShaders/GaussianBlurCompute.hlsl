#define GaussianBlurRS "RootFlags(0)," \
"CBV(b0)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"RootConstants( num32BitConstants = 22, b1 )," \
"RootConstants( num32BitConstants = 1, b2 )," \
"DescriptorTable(SRV(t0, space=3, numDescriptors=1, flags = DESCRIPTORS_VOLATILE))," \
"DescriptorTable(SRV(t0, numDescriptors=unbounded, flags = DESCRIPTORS_VOLATILE))," \
"DescriptorTable(Sampler(s0, numDescriptors=unbounded))"

#include "SqInput.hlsl"
#pragma sq_compute GaussianBlurCS
#pragma sq_rootsig GaussianBlurRS

cbuffer BlurConstant : register(b1)
{
	// blur weight, max up to 7x7 kernel (2*n + 1)
	float4 _TargetSize;	// xy for wh, zw for 1/wh
	float _DepthThreshold;
	float _NormalThreshold;
	int _BlurRadius;
	float padding;

	// d3d packing rule
	float4 _BlurWeightPack[4];
};

static float _BlurWeight[16] = (float[16]) _BlurWeightPack;

cbuffer BlurConstant2 : register(b2)
{
	int _HorizontalBlur;
};

RWTexture2D<float4> _OutputTex : register(u0);
Texture2D _InputTex : register(t0, space3);

void GaussianBlur(uint2 _globalID)
{
	float2 screenUV = (_globalID.xy + .5f) * _TargetSize.zw;
	float2 texOffset;

	if (_HorizontalBlur)
	{
		texOffset = float2(_TargetSize.z, 0.0f);
	}
	else
	{
		texOffset = float2(0.0f, _TargetSize.w);
	}

	float4 color = 0;
	float totalWeight = 0.0f;
	float cDepth = SQ_SAMPLE_TEXTURE_LEVEL(_DepthIndex, _LinearClampSampler, screenUV, 0).r;
	float3 cNormal = SQ_SAMPLE_TEXTURE_LEVEL(_NormalRTIndex, _LinearClampSampler, screenUV, 0).rgb;
	float4 cColor = _InputTex.SampleLevel(_SqSamplerTable[_LinearClampSampler], screenUV, 0);

	// sample within radius
	for (int i = -_BlurRadius; i <= _BlurRadius; i++)
	{
		float2 uv = screenUV + (float)i * texOffset;

		// neighbor depth/normal
		float nDepth = SQ_SAMPLE_TEXTURE_LEVEL(_DepthIndex, _LinearClampSampler, uv, 0).r;
		float3 nNormal = SQ_SAMPLE_TEXTURE_LEVEL(_NormalRTIndex, _LinearClampSampler, uv, 0).rgb;

		// Add neighbor pixel to blur
		if (dot(nNormal, cNormal) >= _NormalThreshold &&
			abs(nDepth - cDepth) <= _DepthThreshold)
		{
			float weight = _BlurWeight[i + _BlurRadius];
			color += weight * _InputTex.SampleLevel(_SqSamplerTable[_LinearClampSampler], uv, 0);
			totalWeight += weight;
		}
	}

	if (totalWeight > FLOAT_EPSILON)
		color /= totalWeight;
	else
		color = cColor;

	_OutputTex[_globalID.xy] = color;
}

[RootSignature(GaussianBlurRS)]
[numthreads(8, 8, 1)]
void GaussianBlurCS(uint3 _globalID : SV_DispatchThreadID)
{
	if (_globalID.x >= _TargetSize.x || _globalID.y >= _TargetSize.y)
		return;

	GaussianBlur(_globalID.xy);
}