#define GaussianBlurRS "RootFlags(0)," \
"CBV(b0)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"RootConstants( num32BitConstants = 1, b2 )," \
"SRV(t1, space=3)," \
"DescriptorTable(SRV(t0, space=3, numDescriptors=1, flags = DESCRIPTORS_VOLATILE))," \
"DescriptorTable(SRV(t0, numDescriptors=unbounded, flags = DESCRIPTORS_VOLATILE))," \
"DescriptorTable(Sampler(s0, numDescriptors=unbounded))"

#include "SqInput.hlsl"
#pragma sq_compute GaussianBlurCS
#pragma sq_rootsig GaussianBlurRS

struct BlurConstant
{
	// blur weight, max up to 7x7 kernel (2*n + 1)
	float2 _InvTargetSize;
	float _DepthThreshold;
	float _NormalThreshold;
	int _BlurRadius;
	float _BlurWeight[15];
};

cbuffer BlurConstant2 : register(b2)
{
	int _HorizontalBlur;
};

RWTexture2D<float4> _OutputTex : register(u0);
Texture2D _InputTex : register(t0, space3);
StructuredBuffer<BlurConstant> _BlurConst : register(t1, space3);

[RootSignature(GaussianBlurRS)]
[numthreads(8, 8, 1)]
void GaussianBlurCS(uint3 _globalID : SV_DispatchThreadID)
{
	float2 screenUV = (_globalID.xy + .5f) * _BlurConst[0]._InvTargetSize;
	float2 texOffset;

	if (_HorizontalBlur)
	{
		texOffset = float2(_BlurConst[0]._InvTargetSize.x, 0.0f);
	}
	else
	{
		texOffset = float2(0.0f, _BlurConst[0]._InvTargetSize.y);
	}

	float4 color = 0;
	float totalWeight = 0.0f;
	//float4 cDepthNormal = SQ_SAMPLE_TEXTURE_LEVEL(_DepthIndex, _LinearClampSampler, screenUV, 0);

	// sample within radius
	for (int i = -_BlurConst[0]._BlurRadius; i <= _BlurConst[0]._BlurRadius; i++)
	{
		float2 uv = screenUV + (float)i * texOffset;
		float weight = _BlurConst[0]._BlurWeight[i + _BlurConst[0]._BlurRadius];
		//float4 nDepthNormal = SQ_SAMPLE_TEXTURE_LEVEL(_DepthIndex, _LinearClampSampler, uv, 0);

		// Add neighbor pixel to blur.
		color += weight * _InputTex.SampleLevel(_SamplerTable[_LinearClampSampler], uv, 0);
		totalWeight += weight;
	}

	color /= totalWeight;
	_OutputTex[_globalID.xy] = color;
}