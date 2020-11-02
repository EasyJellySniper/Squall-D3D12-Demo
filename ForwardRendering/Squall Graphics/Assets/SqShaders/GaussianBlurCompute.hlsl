#define GaussianBlurRS "RootFlags(0)," \
"CBV(b0)," \
"CBV(b1)," \
"RootConstants( num32BitConstants = 1, b2 )," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"DescriptorTable(SRV(t0, space=1, numDescriptors=1))"

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

[numthreads(8, 8, 1)]
void GaussianBlurCS(uint3 _globalID : SV_DispatchThreadID)
{

}