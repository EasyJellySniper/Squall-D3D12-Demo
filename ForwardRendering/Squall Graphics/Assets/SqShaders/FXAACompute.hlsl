#define FXAAComputeRS "RootFlags(0)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"CBV(b0)," \
"CBV(b1)," \
"DescriptorTable(SRV(t0, numDescriptors=1))," \
"DescriptorTable(Sampler(s0 , numDescriptors = unbounded))"

#include "SqInput.hlsl"
#pragma sq_compute FXAAComputeCS
#pragma sq_rootsig FXAAComputeRS

RWTexture2D<float4> _OutputTex : register(u0);
Texture2D _InputTex : register(t0);

cbuffer FXAAConstant : register(b1)
{
	
};

[RootSignature(FXAAComputeRS)]
[numthreads(8,8,1)]
void FXAAComputeCS(uint3 _globalID : SV_DispatchThreadID)
{

}