#include "SqInput.hlsl"
#pragma sq_rayrootsig RTShadowRootSig
#pragma sq_raygen RTShadowRayGen
#pragma sq_closesthit RTShadowClosestHit
#pragma sq_miss RTShadowMiss
#pragma sq_hitgroup RTShadowGroup
#pragma sq_rtshaderconfig RTShadowConfig
#pragma sq_rtpipelineconfig RTShadowPipelineConfig

GlobalRootSignature RTShadowRootSig =
{
    "DescriptorTable( UAV( u0 ) ),"     // raytracing output
    "SRV( t0, space = 2),"              // acceleration strutures
    "DescriptorTable( SRV( t0 , numDescriptors = 1) ),"     // depth map
    "CBV( b1 ),"                        // system constant
    "SRV( t0, space = 1 )"              // sqlight
};

TriangleHitGroup RTShadowGroup =
{
    "",                     // AnyHit
    "RTShadowClosestHit"   // ClosestHit
};

RaytracingShaderConfig RTShadowConfig =
{
    16, // max payload size - 4float
    8   // max attribute size - 2float
};

RaytracingPipelineConfig RTShadowPipelineConfig =
{
    1 // max trace recursion depth
};

struct RayPayload
{
    float atten;
    float3 padding;
};

RaytracingAccelerationStructure _SceneAS : register(t0, space2);
RWTexture2D<float4> _OutputShadow : register(u0);
Texture2D _DepthMap : register(t0);

[shader("raygeneration")]
void RTShadowRayGen()
{
    // center in the middle of the pixel, it's half-offset rule of D3D
    float2 xy = DispatchRaysIndex().xy + 0.5f; 

    // to ndc space
    float2 screenUV = xy / DispatchRaysDimensions().xy * 2.0f - 1.0f;
    screenUV.y = 1 - screenUV.y;

    // depth
    float depth = _DepthMap.Load(uint3(xy, 0)).r;

    [branch]
    if (depth == 0.0f)
    {
        // early out
        return;
    }

    // to world pos
    float3 wpos = DepthToWorldPos(depth, float4(screenUV, 0, 1));

    // setup ray, trace for main dir light
    SqLight mainLight = _SqDirLight[0];

    RayDesc ray;
    ray.Origin = wpos;
    ray.Direction = -mainLight.world.xyz;   // shoot a ray to light
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    // the data payload between ray tracing
    RayPayload payload = { float4(0, 0, 0, 0) };
    TraceRay(_SceneAS, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);

    // output shadow
    _OutputShadow[DispatchRaysIndex().xy] = payload.atten;
}

[shader("closesthit")]
void RTShadowClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // yes yes yes we hit, set shadow atten
    payload.atten = 0.0f;
}

[shader("miss")]
void RTShadowMiss(inout RayPayload payload)
{
    // hit nothing at all, no shadow atten
    payload.atten = 1.0f;
}