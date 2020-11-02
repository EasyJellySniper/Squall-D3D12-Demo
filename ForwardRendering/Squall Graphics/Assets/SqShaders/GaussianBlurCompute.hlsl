#define GaussianBlurRS "RootFlags(0)," \
"CBV(b0)," \
"CBV(b1)," \
"RootConstants( num32BitConstants = 1, b2 )," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"DescriptorTable(SRV(t0, space=1, numDescriptors=1))," \
"DescriptorTable(Sampler(s0, numDescriptors=unbounded))"

#include "SqInput.hlsl"
#pragma sq_compute GaussianBlurCS
#pragma sq_rootsig GaussianBlurRS

cbuffer BlurConstant : register(b1)
{
	// blur weight, max up to 7x7 kernel (2*n + 1)
	float _BlurWeight[15];
	float2 _InvTargetSize;
	float _DepthThreshold;
	float _NormalThreshold;
	int _BlurRadius;
};

cbuffer BlurConstant2 : register(b2)
{
	int _HorizontalBlur;
};

RWTexture2D<float4> _OutputTex : register(u0);
Texture2D _InputTex : register(t0, space1);

[RootSignature(GaussianBlurRS)]
[numthreads(8, 8, 1)]
void GaussianBlurCS(uint3 _globalID : SV_DispatchThreadID)
{
	float2 screenUV = (_globalID.xy + .5f) * _InvTargetSize.xy;
	float2 texOffset;

	if (_HorizontalBlur)
	{
		texOffset = float2(_InvTargetSize.x, 0.0f);
	}
	else
	{
		texOffset = float2(0.0f, _InvTargetSize.y);
	}

	float4 color = 0;
	float totalWeight = 0.0f;

	// sample within radius
	for (int i = -_BlurRadius; i <= _BlurRadius; i++)
	{
		float2 uv = screenUV + (float)i * texOffset;
		float weight = _BlurWeight[i + _BlurRadius];

		// Add neighbor pixel to blur.
		color += weight * _InputTex.SampleLevel(_SamplerTable[_LinearClampSampler], uv, 0);
		totalWeight += weight;
	}

	color /= totalWeight;
	_OutputTex[_globalID.xy] = color;
}