#pragma once
#include <iostream>
#include <unordered_map>
using namespace std;
#include "Mesh.h"
#include <d3d12.h>

class MeshManager
{
public:
	MeshManager(const MeshManager&) = delete;
	MeshManager(MeshManager&&) = delete;
	MeshManager& operator=(const MeshManager&) = delete;
	MeshManager& operator=(MeshManager&&) = delete;

	static MeshManager& Instance()
	{
		static MeshManager instance;
		return instance;
	}

	MeshManager() {}
	~MeshManager() {}

	void Init();
	bool AddMesh(int _instanceID, MeshData _mesh);
	void Release();
	Mesh *GetMesh(int _instanceID);

	vector<D3D12_INPUT_ELEMENT_DESC> &GetDefaultInputLayout();

private:
	unordered_map<int, Mesh> meshes;
	vector<D3D12_INPUT_ELEMENT_DESC> defaultInputLayout;
};