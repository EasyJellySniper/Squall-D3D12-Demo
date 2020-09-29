#ifndef SQRAYINPUT
#define SQRAYINPUT

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

#endif