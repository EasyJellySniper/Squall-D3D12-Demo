// need assign relative path for dxc compiler with forward slash
#define _RAYSHADER
#include "Assets/SqShaders/SqInput.hlsl"
#include "Assets/SqShaders/SqLight.hlsl"

#pragma sq_rayrootsig RTReflectionRootSig
#pragma sq_raygen RTReflectionRayGen
#pragma sq_closesthit RTReflectionClosestHit
#pragma sq_anyhit RTReflectionAnyHit
#pragma sq_miss RTReflectionMiss
#pragma sq_rtshaderconfig RTReflectionConfig
#pragma sq_rtpipelineconfig RTReflectionPipelineConfig

GlobalRootSignature RTReflectionRootSig =
{
    "DescriptorTable( UAV( u0 , numDescriptors = 1) ),"     // raytracing output
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
    2 // max trace recursion depth
};

struct RayPayload
{
    float3 reflectionColor;
    int isTransparent;
};

RaytracingAccelerationStructure _SceneAS : register(t0, space2);
RWTexture2D<float4> _OutputReflection : register(u0);

// bind vb/ib. be careful we use the same heap with texture, indexing to correct address is important
StructuredBuffer<VertexInput> _Vertices[] : register(t0, space3);
ByteAddressBuffer _Indices[] : register(t0, space4);
StructuredBuffer<SubMesh> _SubMesh : register(t0, space5);

uint3 Load3x16BitIndices(uint offsetBytes, uint indexID)
{
    uint3 indices;

    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
    //  Aligned:     { 0 1 | 2 - }
    //  Not aligned: { - 0 | 1 2 }
    const uint dwordAlignedOffset = offsetBytes & ~3;
    const uint2 four16BitIndices = _Indices[indexID].Load2(dwordAlignedOffset);

    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z = four16BitIndices.y & 0xffff;
    }
    else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y = four16BitIndices.y & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }

    return indices;
}

float2 GetHitUV(uint pIdx, uint vertID, BuiltInTriangleIntersectionAttributes attr)
{
    uint indexID = vertID + 1;

    // get indices
    const uint3 indices = Load3x16BitIndices(pIdx, indexID);

    // get uv
    float2 uv[3];
    uv[0] = _Vertices[vertID][indices[0]].uv1;
    uv[1] = _Vertices[vertID][indices[1]].uv1;
    uv[2] = _Vertices[vertID][indices[2]].uv1;

    // interpolate uv according to barycentric coordinate
    return uv[0] +
        attr.barycentrics.x * (uv[1] - uv[0]) +
        attr.barycentrics.y * (uv[2] - uv[0]);
}

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
    ray.TMin = 0.01f;
    ray.TMax = 500;

    // define ray
    TraceRay(_SceneAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

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
    //RayPayload transResult = (RayPayload)0;
    //if (opaqueDepth != transDepth)
    //{
    //    transResult = ShootReflectionRay(transNormal, transDepth, screenUV);
    //}

    // output
    _OutputReflection[DispatchRaysIndex().xy].rgb = opaqueResult.reflectionColor;
}

[shader("closesthit")]
void RTReflectionClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.reflectionColor = 0;
}

[shader("anyhit")]
void RTReflectionAnyHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // mark hit transparent
    payload.isTransparent = 1;

    // accept hit so that system goes to closet hit
    AcceptHitAndEndSearch();
}

[shader("miss")]
void RTReflectionMiss(inout RayPayload payload)
{
    // failed to reflect any thing sample sky color
    payload.reflectionColor = _SkyCube.SampleLevel(_SkySampler, WorldRayDirection(), 0).rgb * _SkyIntensity;
}