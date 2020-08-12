#include "MeshManager.h"
#include "GraphicManager.h"

void MeshManager::Init()
{
	// create defalut input layout
	defaultInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

bool MeshManager::AddMesh(int _instanceID, MeshData _mesh)
{
	// duplicate mesh instance, consider it is added
	if (meshIndexTable.find(_instanceID) != meshIndexTable.end())
	{
		return true;
	}

	Mesh m;
	bool init = m.Initialize(_mesh);
	if (init)
	{
		meshes.push_back(std::move(m));
		meshIndexTable[_instanceID] = (int)meshes.size() - 1;
	}

	return init;
}

void MeshManager::Release()
{
	for (auto&m : meshes)
	{
		m.Release();
	}

	meshes.clear();
	defaultInputLayout.clear();
	meshIndexTable.clear();
	indexInHeap.clear();
}

void MeshManager::CreateBottomAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList)
{
	for (auto& m : meshes)
	{
		m.CreateBottomAccelerationStructure(_dxrList);
	}
}

void MeshManager::ReleaseScratch()
{
	for (auto& m : meshes)
	{
		m.ReleaseScratch();
	}
}

Mesh * MeshManager::GetMesh(int _instanceID)
{
	if (meshIndexTable.find(_instanceID) != meshIndexTable.end())
	{
		return &meshes[meshIndexTable[_instanceID]];
	}

	return nullptr;
}

D3D12_INPUT_ELEMENT_DESC* MeshManager::GetDefaultInputLayout()
{
	return defaultInputLayout.data();
}

UINT MeshManager::GetDefaultInputLayoutSize()
{
	return (UINT)defaultInputLayout.size();
}
