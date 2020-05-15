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

vector<SubMesh> Mesh::GetSubmeshes()
{
	return submeshes;
}
