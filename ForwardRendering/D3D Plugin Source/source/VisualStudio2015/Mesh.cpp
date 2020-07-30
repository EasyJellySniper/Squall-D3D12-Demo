#include "Mesh.h"
#include "GraphicManager.h"
#include "stdafx.h"

bool Mesh::Initialize(MeshData _mesh)
{
	meshData = _mesh;

	// data setup
	for (int i = 0; i < meshData.vertexBufferCount; i++)
	{
		if (meshData.vertexBuffer[i] == nullptr)
		{
			LogMessage(L"[SqGraphic Error] SqMesh: Vertex buffer pointer is null.");
			return false;
		}
		vertexBuffer.push_back((ID3D12Resource*)meshData.vertexBuffer[i]);
	}

	if (meshData.indexBuffer == nullptr)
	{
		LogMessage(L"[SqGraphic Error] SqMesh: Index buffer pointer is null.");
		return false;
	}
	indexBuffer = (ID3D12Resource*)meshData.indexBuffer;

	for (int i = 0; i < meshData.subMeshCount; i++)
	{
		submeshes.push_back(meshData.submesh[i]);
	}

	// create vertex view check
	for (int i = 0; i < meshData.vertexBufferCount; i++)
	{
		D3D12_VERTEX_BUFFER_VIEW vbvDesc;
		vbvDesc.BufferLocation = vertexBuffer[i]->GetGPUVirtualAddress();

		D3D12_RESOURCE_DESC buffDesc = vertexBuffer[i]->GetDesc();
		vbvDesc.SizeInBytes = meshData.vertexSizeInBytes[i];
		vbvDesc.StrideInBytes = meshData.vertexStrideInBytes[i];

		// something wrong when calculating vertex size
		if (vbvDesc.SizeInBytes != buffDesc.Width)
		{
			LogMessage(L"[SqGraphic Error] SqMesh: Vertex buffer size isn't the same as resource. [ " + to_wstring(vbvDesc.SizeInBytes) + L" != " + to_wstring(buffDesc.Width) + L" ]");
			return false;
		}

		vbv.push_back(vbvDesc);
	}

	// index view check
	D3D12_RESOURCE_DESC buffDesc = indexBuffer->GetDesc();
	D3D12_INDEX_BUFFER_VIEW ibvDesc;
	ibvDesc.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	ibvDesc.SizeInBytes = meshData.indexSizeInBytes;
	ibvDesc.Format = (meshData.indexFormat == 0) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	ibv = ibvDesc;

	if (ibvDesc.SizeInBytes != buffDesc.Width)
	{
		LogMessage(L"[SqGraphic Error] SqMesh: Index buffer size isn't the same as resource. [ " + to_wstring(ibvDesc.SizeInBytes) + L" != " + to_wstring(buffDesc.Width) + L" ]");
		return false;
	}

	return true;
}

void Mesh::Release()
{
	submeshes.clear();
	vertexBuffer.clear();
	vbv.clear();
	scratchBottom.reset();
	bottomLevelAS.reset();
}

void Mesh::ReleaseScratch()
{
	scratchBottom.reset();
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView()
{
	if (vbv.size() == 0)
	{
		return D3D12_VERTEX_BUFFER_VIEW();
	}

	return vbv[0];
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
	// create geometry desc
	D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};

	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress();
	geometryDesc.Triangles.IndexCount = meshData.indexSizeInBytes / ((meshData.indexFormat == 0) ? 2 : 4);
	geometryDesc.Triangles.IndexFormat = (meshData.indexFormat == 0) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;		// we only need position (float3)
	geometryDesc.Triangles.VertexCount = meshData.vertexSizeInBytes[0] / meshData.vertexStrideInBytes[0];
	geometryDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer[0]->GetGPUVirtualAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = meshData.vertexStrideInBytes[0];
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

	scratchBottom = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), bottomLevelPrebuildInfo.ScratchDataSizeInBytes, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	bottomLevelAS = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, true, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchBottom->Resource()->GetGPUVirtualAddress();
	bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelAS->Resource()->GetGPUVirtualAddress();

	_dxrList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
}

ID3D12Resource* Mesh::GetBottomAS()
{
	return bottomLevelAS->Resource();
}
