#define RAY_SHADER
#define MAX_REFLECT_RESURSION 3

// need assign relative path for dxc compiler with forward slash
#include "Assets/SqShaders/SqInput.hlsl"
#include "Assets/SqShaders/SqRayInput.hlsl"
#include "Assets/SqShaders/SqLight.hlsl"

#pragma sq_rayrootsig RTReflectionRootSig
#pragma sq_raygen RTReflectionRayGen
#pragma sq_anyhit RTReflectionAnyHit
#pragma sq_closesthit RTReflectionClosestHit
#pragma sq_miss RTReflectionMiss
#pragma sq_rtshaderconfig RTReflectionConfig
#pragma sq_rtpipelineconfig RTReflectionPipelineConfig

// keyword
#pragma sq_keyword _SPEC_GLOSS_MAP
#pragma sq_keyword _EMISSION
#pragma sq_keyword _NORMAL_MAP
#pragma sq_keyword _DETAIL_MAP
#pragma sq_keyword _DETAIL_NORMAL_MAP

GlobalRootSignature RTReflectionRootSig =
{
    "DescriptorTable( UAV( u0 , numDescriptors = 1) ),"     // raytracing output
    "DescriptorTable( UAV( u1 , numDescriptors = 1) ),"     // raytracing output (transparent
    "CBV( b0 ),"                        // system constant
    "SRV( t0, space = 2),"              // acceleration strutures
    "SRV( t0, space = 1 ),"              // sq dir light
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) ),"     // tex table
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, space = 3, flags = DESCRIPTORS_VOLATILE) ),"    //vertex start
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, space = 4, flags = DESCRIPTORS_VOLATILE) ),"    //index start
    "DescriptorTable( Sampler( s0 , numDescriptors = unbounded) ),"     // tex sampler
    "SRV( t0, space = 5),"       // submesh data
    "DescriptorTable( SRV( t0, space = 6, numDescriptors = 1) ),"
    "DescriptorTable( Sampler( s0, space = 6, numDescriptors = 1) )"
};

TriangleHitGroup SqRayHitGroup =
{
    "RTReflectionAnyHit",      // AnyHit
    "RTReflectionClosestHit"   // ClosestHit
};

RaytracingShaderConfig RTReflectionConfig =
{
    24, // max payload size - 6float
    8   // max attribute size - 2float
};

RaytracingPipelineConfig RTReflectionPipelineConfig =
{
    MAX_REFLECT_RESURSION // max trace recursion depth
};

struct RayPayload
{
    float3 reflectionColor;
    float atten;
    float testShadow;
    int reflectionDepth;
};

RWTexture2D<float4> _OutputReflection : register(u0);
RWTexture2D<float4> _OutputReflectionTrans : register(u1);

RayPayload ShootReflectionRay(float3 normal, float depth, float2 screenUV, bool _transparent)
{
    RayPayload payload = (RayPayload)0;
    if (depth == 0.0f)
    {
        return payload;
    }
    payload.reflectionDepth = (_transparent) ? MAX_REFLECT_RESURSION : 0;

    float3 wpos = DepthToWorldPos(depth, float4(screenUV, 0, 1));
    float3 incident = normalize(wpos - _CameraPos.xyz);

    RayDesc ray;
    ray.Origin = wpos;
    ray.Direction = reflect(incident, normal);   // shoot a reflection ray
    ray.TMin = 0;
    ray.TMax = _CameraPos.w;

    // shoot ray & add reflection depth
    payload.reflectionDepth++;
    TraceRay(_SceneAS, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

    return payload;
}

[shader("raygeneration")]
void RTReflectionRayGen()
{
    // reset value
    _OutputReflection[DispatchRaysIndex().xy] = 0;

    // center in the middle of the pixel, it's half-offset rule of D3D
    float2 xy = DispatchRaysIndex().xy + 0.5f;

    // to ndc space
    float2 screenUV = (xy / DispatchRaysDimensions().xy);
    float2 depthUV = screenUV;
    screenUV.y = 1 - screenUV.y;
    screenUV = screenUV * 2.0f - 1.0f;

    // shoot ray according to depth

    float opaqueDepth = SQ_SAMPLE_TEXTURE_LEVEL(_DepthIndex, _CollectShadowSampler, depthUV, 0).r;
    float transDepth = SQ_SAMPLE_TEXTURE_LEVEL(_TransDepthIndex, _CollectShadowSampler, depthUV, 0).r;
    float3 opaqueNormal = SQ_SAMPLE_TEXTURE_LEVEL(_ColorRTIndex, _CollectShadowSampler, depthUV, 0).rgb;
    float3 transNormal = SQ_SAMPLE_TEXTURE_LEVEL(_NormalRTIndex, _CollectShadowSampler, depthUV, 0).rgb;

    RayPayload opaqueResult = ShootReflectionRay(opaqueNormal, opaqueDepth, screenUV, false);
    RayPayload transResult = (RayPayload)0;
    if (opaqueDepth != transDepth)
    {
        transResult = ShootReflectionRay(transNormal, transDepth, screenUV, true);
    }

    // output
    _OutputReflection[DispatchRaysIndex().xy].rgb = opaqueResult.reflectionColor;
    _OutputReflectionTrans[DispatchRaysIndex().xy].rgb = transResult.reflectionColor;
}

float3 SampleSkyForRay(float3 dir)
{
    // prepare sky for blending
    float4x4 mat = SQ_SKYBOX_WORLD;
    mat._13 *= -1;
    mat._31 *= -1;

    dir = mul((float3x3)mat, dir);
    return _SkyCube.SampleLevel(_SkySampler, dir, 0).rgb * _SkyIntensity;
}

RayPayload TraceShadowRay(float3 hitPos)
{
    RayPayload shadowResult = (RayPayload)0;
    shadowResult.atten = 1.0f;
    shadowResult.testShadow = 1.0f;

    // shadow ray, dir light only
    SqLight light = _SqDirLight[0];
    RayDesc shadowRay;

    shadowRay.Origin = hitPos;
    shadowRay.Direction = -light.world.xyz;
    shadowRay.TMin = light.shadowBiasNear;
    shadowRay.TMax = light.shadowDistance;

    TraceRay(_SceneAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE, ~0, 0, 1, 0, shadowRay, shadowResult);

    shadowResult.atten = lerp(1.0f, shadowResult.atten, light.color.a);
    return shadowResult;
}

// the implement is the same as RayTracingShadow.hlsl
void ReceiveShadow(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // mark receive opaque shadow only (for performance)
    payload.atten = 0.0f;
}

[shader("closesthit")]
void RTReflectionClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // if test shadow, early out
    if (payload.testShadow > 0.0f)
    {
        ReceiveShadow(payload, attr);
        return;
    }

    // calc hit pos
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // read indices
    uint ibStride = 2;
    uint pIdx = PrimitiveIndex() * 3 * ibStride + _SubMesh[InstanceIndex()].StartIndexLocation * ibStride;
    uint vertID = InstanceID();
    const uint3 indices = Load3x16BitIndices(pIdx, vertID + 1);

    // init v2f 
    RayV2F v2f = (RayV2F)0;

    // uv
    v2f.tex.xy = GetHitUV(indices, vertID, attr);
    v2f.tex.zw = GetHitUV2(indices, vertID, attr);

    // calc detail uv
    float2 detailUV = lerp(v2f.tex.xy, v2f.tex.zw, _DetailUV);
    detailUV = detailUV * _DetailAlbedoMap_ST.xy + _DetailAlbedoMap_ST.zw;
    v2f.tex.zw = detailUV;
    v2f.tex.xy = v2f.tex.xy *_MainTex_ST.xy + _MainTex_ST.zw;

    // world position & normal
    v2f.worldPos = hitPos;
    v2f.normal = GetHitNormal(indices, vertID, attr);

    // calc TBN for bump mapping
    float4 tangent = GetHitTangent(indices, vertID, attr);
    v2f.worldToTangent = CreateTBN(v2f.normal, tangent);

    // bump normal
    float3 bumpNormal = GetBumpNormal(v2f.tex.xy, v2f.tex.zw, v2f.normal, v2f.worldToTangent);

    // -------------------------------- recursive ray if necessary ------------------------------ //
    RayPayload recursiveResult = (RayPayload)0;
    recursiveResult.reflectionColor = float3(0, 0, 0);

    RayPayload shadowResult = (RayPayload)0;
    shadowResult.atten = 1.0f;

    if (payload.reflectionDepth < MAX_REFLECT_RESURSION && payload.reflectionDepth < _ReflectionCount)
    {
        // shadow ray
        shadowResult = TraceShadowRay(hitPos);

        // recursive ray
        RayDesc recursiveRay;
        recursiveRay.Origin = hitPos;
        recursiveRay.Direction = reflect(WorldRayDirection(), bumpNormal);   // shoot a reflection ray
        recursiveRay.TMin = 0;
        recursiveRay.TMax = _CameraPos.w;

        recursiveResult.reflectionDepth = payload.reflectionDepth + 1;
        TraceRay(_SceneAS, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, 0, 1, 0, recursiveRay, recursiveResult);
    }
    else if (payload.reflectionDepth == 1)
    {
        // trace shadow ray if no recursive
        shadowResult = TraceShadowRay(hitPos);
    }

    float4 result = RayForwardPass(v2f, bumpNormal, recursiveResult.reflectionColor, shadowResult.atten);
    payload.reflectionColor = result.rgb * result.a;
}

[shader("anyhit")]
void RTReflectionAnyHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // read indices
    uint ibStride = 2;
    uint pIdx = PrimitiveIndex() * 3 * ibStride + _SubMesh[InstanceIndex()].StartIndexLocation * ibStride;
    uint vertID = InstanceID();
    const uint3 indices = Load3x16BitIndices(pIdx, vertID + 1);

    // init ray v2f
    RayV2F v2f = (RayV2F)0;

    // uv
    v2f.tex.xy = GetHitUV(indices, vertID, attr);
    v2f.tex.zw = GetHitUV2(indices, vertID, attr);

    // calc detail uv
    float2 detailUV = lerp(v2f.tex.xy, v2f.tex.zw, _DetailUV);
    detailUV = detailUV * _DetailAlbedoMap_ST.xy + _DetailAlbedoMap_ST.zw;
    v2f.tex.zw = detailUV;
    v2f.tex.xy = v2f.tex.xy * _MainTex_ST.xy + _MainTex_ST.zw;

    // alpha test here
    float4 diffuse = GetAlbedo(v2f.tex.xy, v2f.tex.zw);
    if (diffuse.a - _CutOff < 0 && _RenderQueue == 1)
    {
        // cutoff case, discard search
        IgnoreHit();
        return;
    }

    if (diffuse.a < FLOAT_EPSILON && _RenderQueue == 2)
    {
        // discard transparent zero
        IgnoreHit();
        return;
    }

    // accept hit so that system goes to closet hit
    AcceptHitAndEndSearch();
}

[shader("miss")]
void RTReflectionMiss(inout RayPayload payload)
{
    payload.reflectionColor = SampleSkyForRay(WorldRayDirection());
}