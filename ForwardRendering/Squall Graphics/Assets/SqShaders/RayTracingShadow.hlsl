#pragma sq_raygen RTShadowRayGen
#pragma sq_closesthit RTShadowClosestHit
#pragma sq_miss RTShadowMiss

struct RayPayload
{
    float atten;
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