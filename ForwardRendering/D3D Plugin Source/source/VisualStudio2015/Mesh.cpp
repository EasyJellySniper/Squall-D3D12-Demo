#include "Mesh.h"
#include "GraphicManager.h"
#include "stdafx.h"

bool Mesh::Initialize(int _instanceID, MeshData _mesh)
{
	meshData = _mesh;
	instanceID = _instanceID;
	auto device = GraphicManager::Instance().GetDevice();
	auto cmdList = GraphicManager::Instance().GetDxrList();	// bascially creation list

	GraphicManager::Instance().ResetCreationList();

	// data setup
	if (meshData.vertexBuffer == nullptr)
	{
		LogMessage(L"[SqGraphic Error] SqMesh: Vertex buffer pointer is null.");
		return false;
	}

	auto vbSrc = (ID3D12Resource*)meshData.vertexBuffer;
	auto vbDesc = vbSrc->GetDesc();
	vbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;	// remove god damn deny shader flags
	vertexBuffer = make_unique<DefaultBuffer>(device, vbDesc);
	cmdList->CopyResource(vertexBuffer->Resource(), vbSrc);

	if (meshData.indexBuffer == nullptr)
	{
		LogMessage(L"[SqGraphic Error] SqMesh: Index buffer pointer is null.");
		return false;
	}

	auto ibSrc = (ID3D12Resource*)meshData.indexBuffer;
	auto ibDesc = ibSrc->GetDesc();
	ibDesc.Flags = D3D12_RESOURCE_FLAG_NONE;   // remove god damn deny shader flags
	indexBuffer = make_unique<DefaultBuffer>(device, ibDesc);
	cmdList->CopyResource(indexBuffer->Resource(), ibSrc);

	GraphicManager::Instance().ExecuteCreationList();

	for (int i = 0; i < meshData.subMeshCount; i++)
	{
		submeshes.push_back(meshData.submesh[i]);
	}

	// create vertex view check
	D3D12_VERTEX_BUFFER_VIEW vbvDesc;
	vbvDesc.BufferLocation = vertexBuffer->Resource()->GetGPUVirtualAddress();

	D3D12_RESOURCE_DESC buffDesc = vertexBuffer->Resource()->GetDesc();
	vbvDesc.SizeInBytes = meshData.vertexSizeInBytes;
	vbvDesc.StrideInBytes = meshData.vertexStrideInBytes;

	// something wrong when calculating vertex size
	if (vbvDesc.SizeInBytes != buffDesc.Width)
	{
		LogMessage(L"[SqGraphic Error] SqMesh: Vertex buffer size isn't the same as resource. [ " + to_wstring(vbvDesc.SizeInBytes) + L" != " + to_wstring(buffDesc.Width) + L" ]");
		return false;
	}

	vbv = (vbvDesc);

	// index view check
	buffDesc = indexBuffer->Resource()->GetDesc();
	D3D12_INDEX_BUFFER_VIEW ibvDesc;
	ibvDesc.BufferLocation = indexBuffer->Resource()->GetGPUVirtualAddress();
	ibvDesc.SizeInBytes = meshData.indexSizeInBytes;
	ibvDesc.Format = (meshData.indexFormat == 0) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	ibv = ibvDesc;

	if (ibvDesc.SizeInBytes != buffDesc.Width)
	{
		LogMessage(L"[SqGraphic Error] SqMesh: Index buffer size isn't the same as resource. [ " + to_wstring(ibvDesc.SizeInBytes) + L" != " + to_wstring(buffDesc.Width) + L" ]");
		return false;
	}

	// add to texture manager
	int vertCount = vbv.SizeInBytes / vbv.StrideInBytes;
	int idxStride = (meshData.indexFormat == 0) ? 2 : 4;
	int idxCount = ibv.SizeInBytes / idxStride;
	vertexSrv.srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), vertexBuffer->Resource(), TextureInfo(false, false, false, false, true, vertCount, vbv.StrideInBytes));
	indexSrv.srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), indexBuffer->Resource(), TextureInfo(false, false, false, false, true, ibv.SizeInBytes / 4, 0));

	return true;
}

void Mesh::Release()
{
	for (size_t i = 0; i < bottomLevelAS.size(); i++)
	{
		bottomLevelAS[i].reset();
	}

	for (size_t i = 0; i < scratchBottom.size(); i++)
	{
		scratchBottom[i].reset();
	}

	submeshes.clear();
	scratchBottom.clear();
	bottomLevelAS.clear();

	vertexBuffer.reset();
	indexBuffer.reset();
}

void Mesh::ReleaseScratch()
{
	for (size_t i = 0; i < scratchBottom.size(); i++)
	{
		scratchBottom[i].reset();
	}
	scratchBottom.clear();
}

void Mesh::DrawSubMesh(ID3D12GraphicsCommandList* _cmdList, int _subIndex)
{
	SubMesh sm = GetSubMesh(_subIndex);
	_cmdList->DrawIndexedInstanced(sm.IndexCountPerInstance, 1, sm.StartIndexLocation, sm.BaseVertexLocation, 0);
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView()
{
	return vbv;
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView()
{
	return ibv;
}

SubMesh Mesh::GetSubMesh(int _index)
{
	if (_index < 0 || (int)_index >= submeshes.size())
	{
		return SubMesh();
	}

	return submeshes[_index];
}

void Mesh::CreateBottomAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList)
{
	for (int i = 0; i < meshData.subMeshCount; i++)
	{
		// create geometry desc
		D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
		SubMesh sm = meshData.submesh[i];

		UINT ibStride = (meshData.indexFormat == 0) ? 2 : 4;
		geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Triangles.IndexBuffer = indexBuffer->Resource()->GetGPUVirtualAddress() + ibStride * sm.StartIndexLocation;
		geometryDesc.Triangles.IndexCount = sm.IndexCountPerInstance;
		geometryDesc.Triangles.IndexFormat = (meshData.indexFormat == 0) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

		UINT vbStride = meshData.vertexStrideInBytes;
		geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;		// we only need position (float3)
		geometryDesc.Triangles.VertexCount = sm.IndexCountPerInstance * 3;
		geometryDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->Resource()->GetGPUVirtualAddress() + vbStride * sm.BaseVertexLocation;
		geometryDesc.Triangles.VertexBuffer.StrideInBytes = meshData.vertexStrideInBytes;

		geometryDesc.Flags = geometryFlags;

		// create bottom level AS desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;

		bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottomLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		bottomLevelInputs.NumDescs = 1;
		bottomLevelInputs.pGeometryDescs = &geometryDesc;

		// require pre build info
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
		GraphicManager::Instance().GetDxrDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
		if (bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes == 0)
		{
			LogMessage(L"[SqGraphic Error]: Create Bottom Acc Struct Failed.");
			return;
		}

		unique_ptr<DefaultBuffer> scratchBottom = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), bottomLevelPrebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		unique_ptr<DefaultBuffer> bottomLevelAS = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchBottom->Resource()->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelAS->Resource()->GetGPUVirtualAddress();

		_dxrList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		_dxrList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAS->Resource()));

		this->scratchBottom.push_back(move(scratchBottom));
		this->bottomLevelAS.push_back(move(bottomLevelAS));
	}
}

ID3D12Resource* Mesh::GetBottomAS(int _submesh)
{
	return bottomLevelAS[_submesh]->Resource();
}

int Mesh::GetVertexSrv()
{
	return vertexSrv.srv;
}
