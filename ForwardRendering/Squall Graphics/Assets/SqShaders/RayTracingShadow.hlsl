#pragma sq_raygen RTShadowRayGen
#pragma sq_closesthit RTShadowClosestHit
#pragma sq_miss RTShadowMiss
#pragma sq_hitgroup RTShadowGroup
#pragma sq_rtshaderconfig RTShadowConfig
#pragma sq_rtpipelineconfig RTShadowPipelineConfig

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

[shader("raygeneration")]
void RTShadowRayGen()
{
    // @TODO: implement TraceRay here
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