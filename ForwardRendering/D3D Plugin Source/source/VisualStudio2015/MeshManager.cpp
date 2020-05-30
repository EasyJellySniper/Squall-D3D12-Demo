#include "MeshManager.h"

MeshManager::MeshManager()
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
	if (meshes.find(_instanceID) != meshes.end())
	{
		return true;
	}

	Mesh m;
	bool init = m.Initialize(_mesh);
	if (init)
	{
		meshes[_instanceID] = m;
	}

	return init;
}

void MeshManager::Release()
{
	for (auto&m : meshes)
	{
		m.second.Release();
	}
	meshes.clear();

	defaultInputLayout.clear();
}

Mesh * MeshManager::GetMesh(int _instanceID)
{
	if (meshes.find(_instanceID) != meshes.end())
	{
		return &meshes[_instanceID];
	}

	return nullptr;
}

vector<D3D12_INPUT_ELEMENT_DESC> &MeshManager::GetDefaultInputLayout()
{
	return defaultInputLayout;
}
