#define RAY_SHADER

// need assign relative path for dxc compiler with forward slash
#include "Assets/SqShaders/SqInput.hlsl"
#include "Assets/SqShaders/SqRayInput.hlsl"
#include "Assets/SqShaders/SqLight.hlsl"

#pragma sq_rayrootsig RTReflectionRootSig
#pragma sq_raygen RTReflectionRayGen
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
    "",      // AnyHit
    "RTReflectionClosestHit"   // ClosestHit
};

RaytracingShaderConfig RTReflectionConfig =
{
    12, // max payload size - 3float
    8   // max attribute size - 2float
};

RaytracingPipelineConfig RTReflectionPipelineConfig =
{
    2 // max trace recursion depth
};

struct RayPayload
{
    float3 reflectionColor;
};

RWTexture2D<float4> _OutputReflection : register(u0);
RWTexture2D<float4> _OutputReflectionTrans : register(u1);

RayPayload ShootReflectionRay(float3 normal, float depth, float2 screenUV)
{
    RayPayload payload = (RayPayload)0;
    if (depth == 0.0f)
    {
        return payload;
    }

    float3 wpos = DepthToWorldPos(depth, float4(screenUV, 0, 1));
    float3 incident = normalize(wpos - _CameraPos.xyz);

    RayDesc ray;
    ray.Origin = wpos;
    ray.Direction = reflect(incident, normal);   // shoot a reflection ray
    ray.TMin = 0;
    ray.TMax = _CameraPos.w;

    // define ray
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
    float opaqueDepth = _TexTable[_DepthIndex][DispatchRaysIndex().xy].r;
    float transDepth = _TexTable[_TransDepthIndex][DispatchRaysIndex().xy].r;
    float3 opaqueNormal = _TexTable[_ColorRTIndex][DispatchRaysIndex().xy].rgb;
    float3 transNormal = _TexTable[_NormalRTIndex][DispatchRaysIndex().xy].rgb;

    RayPayload opaqueResult = ShootReflectionRay(opaqueNormal, opaqueDepth, screenUV);
    RayPayload transResult = (RayPayload)0;
    if (opaqueDepth != transDepth)
    {
        transResult = ShootReflectionRay(transNormal, transDepth, screenUV);
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

    float4 result = RayForwardPass(v2f);
    float3 sky = _SkyCube.SampleLevel(_SkySampler, WorldRayDirection(), 0).rgb * _SkyIntensity;

    // lerp between sky color & reflection color by alpha
    payload.reflectionColor = lerp(sky, result.rgb, result.a);
}

[shader("miss")]
void RTReflectionMiss(inout RayPayload payload)
{
    // failed to reflect any thing sample sky color
    payload.reflectionColor = _SkyCube.SampleLevel(_SkySampler, WorldRayDirection(), 0).rgb * _SkyIntensity;
}