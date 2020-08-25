// need assign relative path for dxc compiler with forward slash
#include "Assets/SqShaders/SqInput.hlsl"
#pragma sq_rayrootsig RTShadowRootSig
#pragma sq_rayrootsiglocal RTShadowRootSigLocal
#pragma sq_raygen RTShadowRayGen
#pragma sq_closesthit RTShadowClosestHit
#pragma sq_anyhit RTShadowAnyHit
#pragma sq_miss RTShadowMiss
#pragma sq_hitgroup RTShadowGroup
#pragma sq_rtshaderconfig RTShadowConfig
#pragma sq_rtpipelineconfig RTShadowPipelineConfig
#pragma sq_rayrootsigassociation LocalRootSignatureAssociation

GlobalRootSignature RTShadowRootSig =
{
    "DescriptorTable( UAV( u0 , numDescriptors = 1) ),"     // raytracing output
    "CBV( b1 ),"                        // system constant
    "SRV( t0, space = 2),"              // acceleration strutures
    "SRV( t0, space = 1 ),"              // sqlight
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) ),"     // tex table
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, space = 3, flags = DESCRIPTORS_VOLATILE) ),"    //vertex start
    "DescriptorTable( SRV( t0 , numDescriptors = unbounded, space = 4, flags = DESCRIPTORS_VOLATILE) ),"    //index start
    "DescriptorTable( Sampler( s0 , numDescriptors = unbounded) ),"     // tex sampler
    "SRV( t0, space = 5)"       // submesh data
};

LocalRootSignature RTShadowRootSigLocal =
{
    "RootConstants( num32BitConstants = 64, b3 )"   // material constant
};

TriangleHitGroup RTShadowGroup =
{
    "RTShadowAnyHit",      // AnyHit
    "RTShadowClosestHit"   // ClosestHit
};

SubobjectToExportsAssociation LocalRootSignatureAssociation =
{
    "RTShadowRootSigLocal",  // subobject name
    "RTShadowGroup"          // export association 
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
    int isTransparent;
    float distBlockToLight;
    float padding;
};

RaytracingAccelerationStructure _SceneAS : register(t0, space2);
RWTexture2D<float4> _OutputShadow : register(u0);

// bind vb/ib. be careful we use the same heap with texture, indexing to correct address is important
StructuredBuffer<VertexInput> _Vertices[] : register(t0, space3);
ByteAddressBuffer _Indices[] : register(t0, space4);
StructuredBuffer<SubMesh> _SubMesh : register(t0, space5);

void ShootRayFromDepth(float _Depth, float2 _ScreenUV, SqLight _light)
{
    if (_Depth == 0.0f)
    {
        // early out
        return;
    }

    // to world pos
    float3 wpos = DepthToWorldPos(_Depth, float4(_ScreenUV, 0, 1));
    float distToCam = length(_CameraPos.xyz - wpos);

    // setup ray, trace for main dir light

    if (distToCam > _light.cascadeDist[0])
    {
        // save ray if distance is too far
        return;
    }

    float3 lightPos = -_light.world.xyz * _light.cascadeDist[0];

    RayDesc ray;
    ray.Origin = wpos;
    ray.Direction = -_light.world.xyz;   // shoot a ray to light
    ray.TMin = _light.world.w;           // use bias as t min
    ray.TMax = _light.cascadeDist[0];

    // the data payload between ray tracing
    RayPayload payload = { 1.0f, 0, 0, 0 };
    TraceRay(_SceneAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, ~0, 0, 1, 0, ray, payload);

    // lerp between atten and strength
    payload.atten = lerp(1, payload.atten, _light.color.a);

    // output shadow
    float currAtten = _OutputShadow[DispatchRaysIndex().xy].r;
    _OutputShadow[DispatchRaysIndex().xy].r = min(payload.atten, currAtten);

    float receiverDistToLight = length(lightPos - wpos); // receiver dist to light
    _OutputShadow[DispatchRaysIndex().xy].g = receiverDistToLight - payload.distBlockToLight;  // blocker distance to light
    _OutputShadow[DispatchRaysIndex().xy].b = receiverDistToLight;
    _OutputShadow[DispatchRaysIndex().xy].a = _light.shadowSize;    // use as light size
}

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

[shader("raygeneration")]
void RTShadowRayGen()
{
    // reset value
    _OutputShadow[DispatchRaysIndex().xy] = 1;

    // center in the middle of the pixel, it's half-offset rule of D3D
    float2 xy = DispatchRaysIndex().xy + 0.5f;

    // to ndc space
    float2 screenUV = (xy / DispatchRaysDimensions().xy);
    float2 depthUV = screenUV;
    screenUV.y = 1 - screenUV.y;
    screenUV = screenUV * 2.0f - 1.0f;

    // shoot ray according to depth
    float opaqueDepth = _TexTable[_DepthIndex].SampleLevel(_SamplerTable[_CollectShadowSampler], depthUV, 0).r;
    float transDepth = _TexTable[_TransDepthIndex].SampleLevel(_SamplerTable[_CollectShadowSampler], depthUV, 0).r;


    SqLight light = _SqDirLight[0];

    ShootRayFromDepth(opaqueDepth, screenUV, light);
    if (opaqueDepth != transDepth)
    {
        // shoot ray for transparent object if necessary
        ShootRayFromDepth(transDepth, screenUV, light);
    }
}

[shader("closesthit")]
void RTShadowClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // mark atten to 0
    payload.atten = 0.0f;
    payload.distBlockToLight = RayTCurrent();

    float alpha = 1;
    if (payload.isTransparent > 0 || _CutOff > 0)
    {
        // get primitive index
        // offset 3 * sizeof(index) = 6, also consider the startindexlocation
        uint ibStride = 2;
        uint pIdx = PrimitiveIndex() * 3 * ibStride + _SubMesh[InstanceIndex()].StartIndexLocation * ibStride;
        uint vertID = InstanceID();

        // get interpolated uv and tiling it
        float2 uvHit = GetHitUV(pIdx, vertID, attr);
        uvHit = uvHit * _MainTex_ST.xy + _MainTex_ST.zw;
        alpha = _TexTable[_DiffuseIndex].SampleLevel(_SamplerTable[_SamplerIndex], uvHit, 0).a * _Color.a;
    }

    // cutoff
    if (_CutOff > 0)
    {
        // clip shadow
        payload.atten = lerp(1.0f, payload.atten, alpha >= _CutOff);
    }

    // transparent, use invert alpha as shadow atten
    if (payload.isTransparent > 0)
    {
        payload.atten = 1 - alpha;
    }
}

[shader("anyhit")]
void RTShadowAnyHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // mark hit transparent
    payload.isTransparent = 1;

    // accept hit so that system goes to closet hit
    AcceptHitAndEndSearch();
}

[shader("miss")]
void RTShadowMiss(inout RayPayload payload)
{
    // hit nothing at all, no shadow atten
    // doesn't need to do anything
}