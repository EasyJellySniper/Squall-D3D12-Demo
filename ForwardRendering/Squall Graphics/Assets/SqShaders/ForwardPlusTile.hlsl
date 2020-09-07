#define ForwardPlusTileRS "DescriptorTable(UAV(u0, numDescriptors=1))," \
"CBV(b1)," \
"SRV(t1, space=1)," \
"DescriptorTable( SRV( t0 , numDescriptors = unbounded) )," \
"DescriptorTable( Sampler( s0 , numDescriptors = unbounded) )"

#pragma sq_compute ForwardPlusTileCS
#pragma sq_rootsig ForwardPlusTileRS
#include "SqInput.hlsl"

// result
RWByteAddressBuffer _TileResult : register(u0);

// use 32x32 tiles for light culling
[RootSignature(ForwardPlusTileRS)]
[numthreads(32, 32, 1)]
void ForwardPlusTileCS()
{

}