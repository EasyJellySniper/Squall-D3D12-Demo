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
    "DescriptorTable( Sampler( s0 , numDescriptors = unbounded) )"
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
    float2 padding;
};

RaytracingAccelerationStructure _SceneAS : register(t0, space2);
RWTexture2D<float4> _OutputShadow : register(u0);

// bind vb/ib. be careful we use the same heap with texture, indexing to correct address is important
StructuredBuffer<VertexInput> _VertexBuffer[] : register(t0, space3);
StructuredBuffer<IndexInput> _IndexBuffer[] : register(t0, space4);

void ShootRayFromDepth(Texture2D _DepthMap, float2 _ScreenUV)
{
    // depth
    float depth = _DepthMap.Load(uint3(DispatchRaysIndex().xy, 0)).r;

    [branch]
    if (depth == 0.0f)
    {
        // early out
        return;
    }

    // to world pos
    float3 wpos = DepthToWorldPos(depth, float4(_ScreenUV, 0, 1));

    // setup ray, trace for main dir light
    SqLight mainLight = _SqDirLight[0];

    RayDesc ray;
    ray.Origin = wpos;
    ray.Direction = -mainLight.world.xyz;   // shoot a ray to light
    ray.TMin = mainLight.world.w;           // use bias as t min
    ray.TMax = mainLight.cascadeDist[0];

    // the data payload between ray tracing
    RayPayload payload = { 1.0f, 0, 0, 0 };
    TraceRay(_SceneAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, ~0, 0, 1, 0, ray, payload);

    // lerp between atten and strength
    payload.atten = lerp(1, payload.atten, mainLight.color.a);

    // output shadow
    float currAtten = _OutputShadow[DispatchRaysIndex().xy].r;
    _OutputShadow[DispatchRaysIndex().xy] = min(payload.atten, currAtten);
}

float2 GetHitUV(uint pIdx, uint vertID, BuiltInTriangleIntersectionAttributes attr)
{
    uint indexID = vertID + 1;

    // get indices
    uint indice[3];
    indice[0] = _IndexBuffer[indexID][pIdx].index;
    indice[1] = _IndexBuffer[indexID][pIdx + 1].index;
    indice[2] = _IndexBuffer[indexID][pIdx + 2].index;

    // get uv
    float2 uv[3];
    uv[0] = _VertexBuffer[vertID][indice[0]].uv1;
    uv[1] = _VertexBuffer[vertID][indice[1]].uv1;
    uv[2] = _VertexBuffer[vertID][indice[2]].uv1;

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
    screenUV.y = 1 - screenUV.y;
    screenUV = screenUV * 2.0f - 1.0f;

    ShootRayFromDepth(_TexTable[_DepthIndex], screenUV);
    ShootRayFromDepth(_TexTable[_TransDepthIndex], screenUV);
}

[shader("closesthit")]
void RTShadowClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // transparent, use invert alpha as shadow atten
    if (payload.isTransparent > 0)
    {
        payload.atten = 1 - _Color.a;
        return;
    }

    // opaque shadow
    payload.atten = 0.0f;

    // cutoff
    if (_CutOff > 0)
    {
        // get primitive index
        uint pIdx = PrimitiveIndex();
        uint vertID = InstanceID();

        // get interpolated uv and tiling it
        float2 uvHit = GetHitUV(pIdx, vertID, attr);
        uvHit = uvHit * _MainTex_ST.xy + _MainTex_ST.zw;

        // clip shadow
        float alpha = _TexTable[_DiffuseIndex].SampleLevel(_SamplerTable[_SamplerIndex], uvHit, 0).a * _Color.a;
        payload.atten = lerp(1.0f, payload.atten, alpha >= _CutOff);
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