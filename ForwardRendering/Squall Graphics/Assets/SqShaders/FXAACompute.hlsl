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
	float4 targetSize;
};

[RootSignature(FXAAComputeRS)]
[numthreads(8,8,1)]
void FXAAComputeCS(uint3 _globalID : SV_DispatchThreadID)
{
	if (_globalID.x >= targetSize.x || _globalID.y >= targetSize.y)
	{
		return;
	}

	float4 col = _InputTex[_globalID.xy];
	col.rgb = RgbToLuma(col.rgb);

	_OutputTex[_globalID.xy] = col;
}