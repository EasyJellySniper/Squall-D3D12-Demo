#define RAY_SHADER

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
    "CBV( b1 ),"    // ambient constant
    "SRV( t0, space = 2),"              // acceleration strutures
    "SRV( t0, space = 1 ),"              // sq dir light
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) ),"     // tex table
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, space = 3, flags = DESCRIPTORS_VOLATILE) ),"    //vertex start
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, space = 4, flags = DESCRIPTORS_VOLATILE) ),"    //index start
    "DescriptorTable( Sampler( s0 , numDescriptors = unbounded) ),"     // tex sampler
    "SRV( t0, space = 5),"       // submesh data
    "SRV( t0, space = 6)"       // uniform vector
};

TriangleHitGroup SqRayHitGroup =
{
    "",      // AnyHit
    "RTAmbientClosestHit"   // ClosestHit
};

RaytracingShaderConfig RTAmbientConfig =
{
    20, // max payload size
    8   // max attribute size - 2float
};

RaytracingPipelineConfig RTAmbientPipelineConfig =
{
    1 // max trace recursion depth
};

struct RayPayload
{
    float4 ambientColor;
    bool isHit;
};

cbuffer AmbientData : register(b1)
{
    float _AmbientDiffuseDistance;
    float _AmbientOcclusionDistance;
    int _SampleCount;
    int _AmbientNoiseIndex;
};

RWTexture2D<float4> _OutputAmbient : register(u0);
StructuredBuffer<float4> _UniformVector : register(t0, space6);

float3 GetRandomVector(uint idx, float2 uv)
{
    // tiling noise sample
    float3 randVec = SQ_SAMPLE_TEXTURE_LEVEL(_AmbientNoiseIndex, _AnisotropicSampler, 4 * uv, 0).rgb;
    randVec = randVec * 2.0f - 1.0f;

    float3 offset = reflect(_UniformVector[idx].xyz, randVec);
    return offset;
}

[shader("raygeneration")]
void RTAmbientRayGen()
{
    // clear
    _OutputAmbient[DispatchRaysIndex().xy] = float4(0, 0, 0, 1);

    // center in the middle of the pixel, it's half-offset rule of D3D
    float2 xy = DispatchRaysIndex().xy + 0.5f;
    float2 screenUV = (xy / DispatchRaysDimensions().xy);
    float2 depthUV = screenUV;
    screenUV.y = 1 - screenUV.y;
    screenUV = screenUV * 2.0f - 1.0f;

    float depth = SQ_SAMPLE_TEXTURE_LEVEL(_DepthIndex, _AnisotropicSampler, depthUV, 0).r;
    if (depth == 0.0f)
    {
        // early out
        return;
    }

    float3 wpos = DepthToWorldPos(float4(screenUV, depth, 1));
    float3 normal = SQ_SAMPLE_TEXTURE_LEVEL(_NormalRTIndex, _AnisotropicSampler, depthUV, 0).rgb;

    float3 diffuse = 0;
    int hitCount = 0;

    for (uint n = 0; n < _SampleCount; n++)
    {
        float3 dir = GetRandomVector(n, depthUV);
        dir.z = lerp(-dir.z, dir.z, sign(dir.z) == sign(normal.z)); // make z dir the same side as normal
        dir = normalize(dir + normal);

        // define ray
        RayDesc ray;
        ray.Origin = wpos;
        ray.Direction = dir;
        ray.TMin = 0.001f;
        ray.TMax = _AmbientDiffuseDistance;

        // setup payload & shoot
        RayPayload payload = (RayPayload)0;
        payload.ambientColor.a = 1.0f;
        TraceRay(_SceneAS, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES | RAY_FLAG_CULL_NON_OPAQUE, ~0, 0, 1, 0, ray, payload);

        // add result
        if (payload.isHit)
        {
            diffuse += payload.ambientColor.rgb;
            hitCount++;
        }
    }

    if (hitCount > 0)
    {
        diffuse /= hitCount;
        _OutputAmbient[DispatchRaysIndex().xy].rgb = diffuse;
    }
}

[shader("closesthit")]
void RTAmbientClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.isHit = true;

    // hit pos
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // init v2f
    RayV2F v2f = InitRayV2F(attr, hitPos);

    // output diffuse
    float3 bumpNormal = GetBumpNormal(v2f.tex.xy, v2f.tex.zw, v2f.normal, v2f.worldToTangent);
    float3 diffuse = RayDiffuse(v2f, bumpNormal);
    payload.ambientColor.rgb = diffuse;
}

[shader("miss")]
void RTAmbientMiss(inout RayPayload payload)
{
    // missing hit, do nothing
}