#define GaussianBlurRS "RootFlags(0)," \
"CBV(b0)," \
"CBV(b1)," \
"DescriptorTable(UAV(u0, numDescriptors=1))," \
"DescriptorTable(SRV(t0, numDescriptors=1))"

#include "SqInput.hlsl"
#pragma sq_compute GaussianBlurCS
#pragma sq_rootsig GaussianBlurRS

cbuffer BlurConstant : register(b1)
{

};

Texture2D _InputTex : register(t0);
RWTexture2D<float4> _OutputTex : register(u0);

[numthreads(8, 8, 1)]
void GaussianBlurCS(uint3 _globalID : SV_DispatchThreadID)
{

}