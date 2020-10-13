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
    "DescriptorTable(SRV(t3, space = 1, numDescriptors=1)),"    // point light tile result (opaque)
    "DescriptorTable(SRV(t4, space = 1, numDescriptors=1)),"    // point light tile result (transparent)
    "SRV( t0, space = 2),"              // acceleration strutures
    "SRV( t0, space = 1 ),"              // sq dir light
    "SRV( t1, space = 1 ),"             // sq point light
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
    16, // max payload size - 4float
    8   // max attribute size - 2float
};

RaytracingPipelineConfig RTReflectionPipelineConfig =
{
    MAX_REFLECT_RESURSION // max trace recursion depth
};

struct RayPayload
{
    float3 reflectionColor;
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

[shader("closesthit")]
void RTReflectionClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
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
    v2f.normal = mul((float3x3)ObjectToWorld3x4(), GetHitNormal(indices, vertID, attr));

    // calc TBN for bump mapping
    float4 tangent = GetHitTangent(indices, vertID, attr);
    tangent.xyz = mul((float3x3)ObjectToWorld3x4(), tangent.xyz);
    v2f.worldToTangent = CreateTBN(v2f.normal, tangent);

    // bump normal
    float3 bumpNormal = GetBumpNormal(v2f.tex.xy, v2f.tex.zw, v2f.normal, v2f.worldToTangent);

    // -------------------------------- recursive ray if necessary ------------------------------ //
    RayPayload recursiveResult = (RayPayload)0;
    recursiveResult.reflectionColor = float3(0, 0, 0);

    if (payload.reflectionDepth < MAX_REFLECT_RESURSION && payload.reflectionDepth < _ReflectionCount)
    {
        RayDesc recursiveRay;
        recursiveRay.Origin = hitPos;
        recursiveRay.Direction = reflect(WorldRayDirection(), bumpNormal);   // shoot a reflection ray
        recursiveRay.TMin = 0;
        recursiveRay.TMax = _CameraPos.w;

        recursiveResult.reflectionDepth = payload.reflectionDepth + 1;
        TraceRay(_SceneAS, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, 0, 1, 0, recursiveRay, recursiveResult);
    }

    float4 result = RayForwardPass(v2f, bumpNormal);

    // prepare sky for blending
    float4x4 mat = SQ_SKYBOX_WORLD;
    mat._13 *= -1;
    mat._31 *= -1;

    float3 dir = mul((float3x3)mat, WorldRayDirection());
    float3 sky = _SkyCube.SampleLevel(_SkySampler, dir, 0).rgb * _SkyIntensity;

    // lerp between sky color & reflection color by alpha
    payload.reflectionColor = lerp(sky, result.rgb, result.a) + recursiveResult.reflectionColor;
}

[shader("anyhit")]
void RTReflectionAnyHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // set max reflection resursion, I don't resursive transparent
    payload.reflectionDepth = MAX_REFLECT_RESURSION;

    // accept hit so that system goes to closet hit
    AcceptHitAndEndSearch();
}

[shader("miss")]
void RTReflectionMiss(inout RayPayload payload)
{
    // failed to reflect any thing sample sky color
    float4x4 mat = SQ_SKYBOX_WORLD;
    mat._13 *= -1;
    mat._31 *= -1;

    float3 dir = mul((float3x3)mat, WorldRayDirection());
    payload.reflectionColor = _SkyCube.SampleLevel(_SkySampler, dir, 0).rgb * _SkyIntensity;
}