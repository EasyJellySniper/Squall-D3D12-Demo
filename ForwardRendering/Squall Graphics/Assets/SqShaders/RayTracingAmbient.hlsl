#define RAY_SHADER
#define MAX_AMBIENT_RESURSION 4
#define LIGHT_PULL_BACK 500

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
    // center in the middle of the pixel, it's half-offset rule of D3D
    float2 xy = DispatchRaysIndex().xy + 0.5f;
    float2 screenUV = (xy / DispatchRaysDimensions().xy);
    float2 depthUV = screenUV;
    screenUV.y = 1 - screenUV.y;
    screenUV = screenUV * 2.0f - 1.0f;

    float depth = SQ_SAMPLE_TEXTURE_LEVEL(_DepthIndex, _CollectShadowSampler, depthUV, 0).r;
    if (depth == 0.0f)
    {
        // early out
        return;
    }

    float3 wpos = DepthToWorldPos(float4(screenUV, depth, 1));

    // pullback to  main light
    SqLight dirLight = _SqDirLight[0];
    float3 origin = wpos - dirLight.world.xyz * LIGHT_PULL_BACK;

    // define ray
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dirLight.world.xyz;
    ray.TMin = 0;
    ray.TMax = LIGHT_PULL_BACK * 1.1f;

    // setup payload, alpha channel for occlusion
    RayPayload payload = (RayPayload)0;
    payload.ambientColor.a = 1.0f;

    // trace ray & culling non opaque
    payload.ambientDepth++;
    TraceRay(_SceneAS, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES | RAY_FLAG_CULL_NON_OPAQUE, ~0, 0, 1, 0, ray, payload);
}

[shader("closesthit")]
void RTAmbientClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // hit pos
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // init v2f
    RayV2F v2f = InitRayV2F(attr, hitPos);

    float3 bumpNormal = GetBumpNormal(v2f.tex.xy, v2f.tex.zw, v2f.normal, v2f.worldToTangent);
    float3 diffuse = RayDiffuse(v2f, bumpNormal);
    SqLight dirLight = _SqDirLight[0];

    // -------------------------------- output diffuse if necessary ------------------------------ //
    if (payload.ambientDepth > 1)
    {
        float2 screenPos = WorldToScreenPos(hitPos, (float2)DispatchRaysDimensions().xy);
        _OutputAmbient[screenPos].rgb += diffuse;
    }

    // -------------------------------- recursive ray if necessary ------------------------------ //
    if (payload.ambientDepth < MAX_AMBIENT_RESURSION && payload.ambientDepth <= dirLight.bounceCount)
    {
        // define indirect ray
        RayDesc ray;
        ray.Origin = hitPos;
        ray.Direction = reflect(WorldRayDirection(), bumpNormal);
        ray.TMin = 0;
        ray.TMax = dirLight.world.w + 0.5f;

        // setup payload & shoot indirect ray
        payload.ambientColor.rgb = diffuse;
        payload.ambientDepth++;
        TraceRay(_SceneAS, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES | RAY_FLAG_CULL_NON_OPAQUE, ~0, 0, 1, 0, ray, payload);
    }
}

[shader("miss")]
void RTAmbientMiss(inout RayPayload payload)
{
    // missing hit, do nothing
}