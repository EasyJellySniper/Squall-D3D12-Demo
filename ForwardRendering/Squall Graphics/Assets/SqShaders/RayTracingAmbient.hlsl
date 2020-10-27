#define RAY_SHADER
#define MAX_AMBIENT_RESURSION 3

// need assign relative path for dxc compiler with forward slash
#include "Assets/SqShaders/SqInput.hlsl"
#include "Assets/SqShaders/SqRayInput.hlsl"
#include "Assets/SqShaders/SqLight.hlsl"

#pragma sq_rayrootsig RTAmbientRootSig
#pragma sq_raygen RTAmbientRayGen
#pragma sq_closesthit RTAmbientClosestHit
#pragma sq_miss RTAmbientMiss
#pragma sq_rtshaderconfig RTAmbientConfig
#pragma sq_rtpipelineconfig RTAmbientPipelineConfig

// keyword
#pragma sq_keyword _SPEC_GLOSS_MAP
#pragma sq_keyword _EMISSION
#pragma sq_keyword _NORMAL_MAP
#pragma sq_keyword _DETAIL_MAP
#pragma sq_keyword _DETAIL_NORMAL_MAP

GlobalRootSignature RTAmbientRootSig =
{
    "DescriptorTable( UAV( u0 , numDescriptors = 1) ),"     // raytracing output
    "CBV( b0 ),"                        // system constant
    "SRV( t0, space = 2),"              // acceleration strutures
    "SRV( t0, space = 1 ),"              // sq dir light
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) ),"     // tex table
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, space = 3, flags = DESCRIPTORS_VOLATILE) ),"    //vertex start
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, space = 4, flags = DESCRIPTORS_VOLATILE) ),"    //index start
    "DescriptorTable( Sampler( s0 , numDescriptors = unbounded) ),"     // tex sampler
    "SRV( t0, space = 5)"       // submesh data
};

TriangleHitGroup SqRayHitGroup =
{
    "",      // AnyHit
    "RTAmbientClosestHit"   // ClosestHit
};

RaytracingShaderConfig RTAmbientConfig =
{
    20, // max payload size - 5float
    8   // max attribute size - 2float
};

RaytracingPipelineConfig RTAmbientPipelineConfig =
{
    MAX_AMBIENT_RESURSION // max trace recursion depth
};

struct RayPayload
{
    float4 ambientColor;
    int ambientDepth;
};

RWTexture2D<float4> _OutputAmbient : register(u0);

[shader("raygeneration")]
void RTAmbientRayGen()
{

}

[shader("closesthit")]
void RTAmbientClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{

}

[shader("miss")]
void RTAmbientMiss(inout RayPayload payload)
{
    
}