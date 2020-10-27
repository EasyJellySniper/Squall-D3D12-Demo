#ifndef SQRAYINPUT
#define SQRAYINPUT

#include "Assets/SqShaders/SqForwardInclude.hlsl"
#include "Assets/SqShaders/SqLight.hlsl"

// ray tracing hit group & local rootsignature, share definition for all ray tracing shader
#pragma sq_hitgroup SqRayHitGroup
#pragma sq_rayrootsiglocal SqRootSigLocal
#pragma sq_rayrootsigassociation SqLocalRootSignatureAssociation

// ray tracing local root signature
LocalRootSignature SqRootSigLocal =
{
	"RootConstants( num32BitConstants = 64, b2 )"   // material constant
};

// link local root signature
SubobjectToExportsAssociation SqLocalRootSignatureAssociation =
{
	"SqRootSigLocal",  // subobject name
	"SqRayHitGroup"          // export association 
};

RaytracingAccelerationStructure _SceneAS : register(t0, space2);
// bind vb/ib. be careful we use the same heap with texture, indexing to correct address is important
StructuredBuffer<VertexInput> _Vertices[] : register(t0, space3);
ByteAddressBuffer _Indices[] : register(t0, space4);
StructuredBuffer<SubMesh> _SubMesh : register(t0, space5);

struct RayV2F
{
    float4 tex;
    float3 worldPos;
    float3 normal;
    float3x3 worldToTangent;
};

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

float2 GetHitUV(uint3 indices, uint vertID, BuiltInTriangleIntersectionAttributes attr)
{
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

float2 GetHitUV2(uint3 indices, uint vertID, BuiltInTriangleIntersectionAttributes attr)
{
    // get uv
    float2 uv[3];
    uv[0] = _Vertices[vertID][indices[0]].uv2;
    uv[1] = _Vertices[vertID][indices[1]].uv2;
    uv[2] = _Vertices[vertID][indices[2]].uv2;

    // interpolate uv according to barycentric coordinate
    return uv[0] +
        attr.barycentrics.x * (uv[1] - uv[0]) +
        attr.barycentrics.y * (uv[2] - uv[0]);
}

float3 GetHitNormal(uint3 indices, uint vertID, BuiltInTriangleIntersectionAttributes attr)
{
    // get uv
    float3 normal[3];
    normal[0] = _Vertices[vertID][indices[0]].normal;
    normal[1] = _Vertices[vertID][indices[1]].normal;
    normal[2] = _Vertices[vertID][indices[2]].normal;

    // interpolate normal according to barycentric coordinate
    return normal[0] +
        attr.barycentrics.x * (normal[1] - normal[0]) +
        attr.barycentrics.y * (normal[2] - normal[0]);
}

float4 GetHitTangent(uint3 indices, uint vertID, BuiltInTriangleIntersectionAttributes attr)
{
    // get uv
    float4 tangent[3];
    tangent[0] = _Vertices[vertID][indices[0]].tangent;
    tangent[1] = _Vertices[vertID][indices[1]].tangent;
    tangent[2] = _Vertices[vertID][indices[2]].tangent;

    // interpolate tangent according to barycentric coordinate
    return tangent[0] +
        attr.barycentrics.x * (tangent[1] - tangent[0]) +
        attr.barycentrics.y * (tangent[2] - tangent[0]);
}

// forward pass for ray tracing
// the calculation should be sync with ForwardPass.hlsl
float4 RayForwardPass(RayV2F i, float3 bumpNormal, float3 indirectSpecular, float atten)
{
    float4 diffuse = GetAlbedo(i.tex.xy, i.tex.zw);

    // specular
    float4 specular = GetSpecular(i.tex.xy);
    diffuse.rgb = DiffuseAndSpecularLerp(diffuse.rgb, specular.rgb);

    // normal
    float occlusion = GetOcclusion(i.tex.xy);
    SqGI gi = CalcGI(bumpNormal, 0, 0, occlusion, false);
    gi.indirectSpecular = indirectSpecular * occlusion;

    // BRDF
    diffuse.rgb = LightBRDFSimple(diffuse.rgb, specular.rgb, specular.a, bumpNormal, i.worldPos, atten, gi);

    // emission
    float3 emission = GetEmission(i.tex.xy);

    float4 output = diffuse;
    output.rgb += emission;

    // adjust alpha
    output.a = lerp(output.a, 1, _RenderQueue < 2);

    return output;
}

RayV2F InitRayV2F(BuiltInTriangleIntersectionAttributes attr, float3 hitPos)
{
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
    v2f.tex.xy = v2f.tex.xy * _MainTex_ST.xy + _MainTex_ST.zw;

    // world position & normal
    v2f.worldPos = hitPos;
    v2f.normal = GetHitNormal(indices, vertID, attr);

    // calc TBN for bump mapping
    float4 tangent = GetHitTangent(indices, vertID, attr);
    v2f.worldToTangent = CreateTBN(v2f.normal, tangent);

    return v2f;
}

#endif