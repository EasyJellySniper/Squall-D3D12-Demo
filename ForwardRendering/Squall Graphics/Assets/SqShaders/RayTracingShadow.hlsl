// need assign relative path for dxc compiler with forward slash
#include "Assets/SqShaders/SqInput.hlsl"
#include "Assets/SqShaders/SqRayInput.hlsl"
#include "Assets/SqShaders/SqLight.hlsl"

#pragma sq_rayrootsig RTShadowRootSig
#pragma sq_raygen RTShadowRayGen
#pragma sq_closesthit RTShadowClosestHit
#pragma sq_anyhit RTShadowAnyHit
#pragma sq_miss RTShadowMiss
#pragma sq_rtshaderconfig RTShadowConfig
#pragma sq_rtpipelineconfig RTShadowPipelineConfig

GlobalRootSignature RTShadowRootSig =
{
    "DescriptorTable( UAV( u0 , numDescriptors = 1) ),"     // raytracing output
    "DescriptorTable( UAV( u1 , numDescriptors = 1) ),"     // raytracing output transparent
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
    "SRV( t0, space = 5)"       // submesh data
};

TriangleHitGroup SqRayHitGroup =
{
    "RTShadowAnyHit",      // AnyHit
    "RTShadowClosestHit"   // ClosestHit
};

RaytracingShaderConfig RTShadowConfig =
{
    12, // max payload size - 4float
    8   // max attribute size - 2float
};

RaytracingPipelineConfig RTShadowPipelineConfig =
{
    1 // max trace recursion depth
};

struct RayPayload
{
    float atten;
    float distBlockToLight;
    float padding;
};

struct RayResult
{
    float atten;
    float distBlockToLight;
    float distReceiverToLight;
    float lightSize;
    float lightAtten;
    bool pointLightRange;
};

RWTexture2D<float4> _OutputShadow : register(u0);
RWTexture2D<float4> _OutputShadowTrans : register(u1);

RayResult ShootRayFromDepth(float _Depth, float3 _Normal, float2 _ScreenUV, SqLight _light, RayResult _result)
{
    if (_Depth == 0.0f)
    {
        // early out
        return _result;
    }

    // to world pos
    float3 wpos = DepthToWorldPos(float4(_ScreenUV, _Depth, 1));
    float3 lightPos = (_light.type == 1) ? -_light.world.xyz * _light.shadowDistance : _light.world.xyz;
    float distToCam = length(_CameraPos.xyz - wpos);
    float receiverDistToLight = length(lightPos - wpos);

    if (receiverDistToLight > _light.range)
    {
        return _result;
    }

    if (_light.type != 1)
    {
        _result.pointLightRange = true;
        _result.lightAtten = max(LightAtten(_light.type, receiverDistToLight, _light.range, true), _result.lightAtten);
    }

    if (distToCam > _light.shadowDistance)
    {
        // save ray if distance is too far
        return _result;
    }

    // normal check if it isn't dir light
    if (_light.type != 1)
    {
        float3 dir = normalize(lightPos - wpos);
        if (dot(dir, _Normal) < 0.0f)
        {
            // don't trace ray from back side
            return _result;
        }
    }

    RayDesc ray;
    ray.Origin = wpos;
    ray.Direction = (_light.type == 1) ? -_light.world.xyz : -normalize(wpos - lightPos);   // shoot a ray to light according to light type
    ray.TMin = _light.shadowBiasNear;
    ray.TMax = (_light.type == 1) ? _light.shadowDistance : receiverDistToLight - _light.shadowBiasFar;

    // the data payload between ray tracing
    RayPayload payload = { 1.0f, 0, 0 };
    TraceRay(_SceneAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, ~0, 0, 1, 0, ray, payload);

    // lerp between atten and strength
    payload.atten = lerp(1, payload.atten, _light.color.a);

    // output to result (use min/max to combine multi lights)
    _result.atten = min(_result.atten, payload.atten);
    _result.distBlockToLight = min(receiverDistToLight - payload.distBlockToLight, _result.distBlockToLight);
    _result.distReceiverToLight = min(receiverDistToLight, _result.distReceiverToLight);
    _result.lightSize = min(_light.shadowSize, _result.lightSize);

    return _result;
}

RayResult GetInitRayResult()
{
    RayResult result = (RayResult)FLOAT_MAX;
    result.atten = 1.0f;
    result.pointLightRange = false;
    result.lightAtten = 0.0f;
    return result;
}

RayResult TraceDirLight(float depth, float2 screenUV)
{
    RayResult result = GetInitRayResult();

    // dir light
    SqLight light = _SqDirLight[0];
    result = ShootRayFromDepth(depth, 0, screenUV, light, result);

    return result;
}

RayResult TracePointLight(ByteAddressBuffer tileData, int tileOffset, float depth, float3 normal, float2 screenUV)
{
    RayResult result = GetInitRayResult();

    uint tileCount = tileData.Load(tileOffset);
    uint offset = tileOffset + 4;

    // trace opaque
    for (uint i = 0; i < tileCount; i++)
    {
        uint idx = tileData.Load(offset);
        SqLight light = _SqPointLight[idx];

        result = ShootRayFromDepth(depth, normal, screenUV, light, result);
        offset += 4;
    }

    return result;
}

[shader("raygeneration")]
void RTShadowRayGen()
{
    // reset value
    _OutputShadow[DispatchRaysIndex().xy] = 1;
    _OutputShadowTrans[DispatchRaysIndex().xy] = 1;

    // center in the middle of the pixel, it's half-offset rule of D3D
    float2 xy = DispatchRaysIndex().xy + 0.5f;

    // to ndc space
    float2 screenUV = (xy / DispatchRaysDimensions().xy);
    float2 depthUV = screenUV;
    screenUV.y = 1 - screenUV.y;
    screenUV = screenUV * 2.0f - 1.0f;

    // shoot ray according to depth
    float opaqueDepth = _SqTexTable[_DepthIndex][DispatchRaysIndex().xy].r;
    float transDepth = _SqTexTable[_TransDepthIndex][DispatchRaysIndex().xy].r;
    float3 opaqueNormal = _SqTexTable[_NormalRTIndex][DispatchRaysIndex().xy].rgb;
    float3 transNormal = _SqTexTable[_TransNormalRTIndex][DispatchRaysIndex().xy].rgb;

    // get forward+ tile
    uint tileX = DispatchRaysIndex().x / TILE_SIZE;
    uint tileY = DispatchRaysIndex().y / TILE_SIZE;
    uint tileIndex = tileX + tileY * _TileCountX;
    int tileOffset = GetPointLightOffset(tileIndex);

    // trace opaque
    RayResult dirResult = TraceDirLight(opaqueDepth, screenUV);
    RayResult pointResult = TracePointLight(_SqPointLightTile, tileOffset, opaqueDepth, opaqueNormal, screenUV);

    if (pointResult.pointLightRange)
    {
        dirResult.atten = lerp(dirResult.atten, pointResult.atten, pointResult.lightAtten);
        dirResult.distBlockToLight = lerp(dirResult.distBlockToLight, pointResult.distBlockToLight, pointResult.lightAtten);
        dirResult.distReceiverToLight = lerp(dirResult.distReceiverToLight, pointResult.distReceiverToLight, pointResult.lightAtten);
        dirResult.lightSize = lerp(dirResult.lightSize, pointResult.lightSize, pointResult.lightAtten);
    }

    // output opaque
    _OutputShadow[DispatchRaysIndex().xy] = float4(dirResult.atten, dirResult.distBlockToLight, dirResult.distReceiverToLight, dirResult.lightSize);

    // trace transparent if necessary
    if (opaqueDepth != transDepth)
    {
        dirResult = TraceDirLight(transDepth, screenUV);
        pointResult = TracePointLight(_SqPointLightTransTile, tileOffset, transDepth, transNormal, screenUV);

        if (pointResult.pointLightRange)
        {
            dirResult.atten = lerp(dirResult.atten, pointResult.atten, pointResult.lightAtten);
            dirResult.distBlockToLight = lerp(dirResult.distBlockToLight, pointResult.distBlockToLight, pointResult.lightAtten);
            dirResult.distReceiverToLight = lerp(dirResult.distReceiverToLight, pointResult.distReceiverToLight, pointResult.lightAtten);
            dirResult.lightSize = lerp(dirResult.lightSize, pointResult.lightSize, pointResult.lightAtten);
        }

        // output transparent
        _OutputShadowTrans[DispatchRaysIndex().xy] = float4(dirResult.atten, dirResult.distBlockToLight, dirResult.distReceiverToLight, dirResult.lightSize);
    }
}

[shader("closesthit")]
void RTShadowClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // mark atten to 0
    payload.atten = 0.0f;
    payload.distBlockToLight = RayTCurrent();
}

[shader("anyhit")]
void RTShadowAnyHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float alpha = 1;

    // get primitive index
    // offset 3 * sizeof(index) = 6, also consider the startindexlocation
    uint ibStride = 2;
    uint pIdx = PrimitiveIndex() * 3 * ibStride + _SubMesh[InstanceIndex()].StartIndexLocation * ibStride;
    uint vertID = InstanceID();
    const uint3 indices = Load3x16BitIndices(pIdx, vertID + 1);

    // get interpolated uv and tiling it
    float2 uvHit = GetHitUV(indices, vertID, attr);
    uvHit = uvHit * _MainTex_ST.xy + _MainTex_ST.zw;
    alpha = SQ_SAMPLE_TEXTURE_LEVEL(_DiffuseIndex, _SamplerIndex, uvHit, 0).a * _Color.a;

    // cutoff
    if (alpha - _CutOff < 0 && _RenderQueue == 1)
    {
        IgnoreHit();
        return;
    }

    // transparent
    if (alpha < FLOAT_EPSILON && _RenderQueue == 2)
    {
        IgnoreHit();
        return;
    }

    // accept hit so that system goes to closet hit
    AcceptHitAndEndSearch();
}

[shader("miss")]
void RTShadowMiss(inout RayPayload payload)
{
    // hit nothing at all, no shadow atten
    // doesn't need to do anything
}