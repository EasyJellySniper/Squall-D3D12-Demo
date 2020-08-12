#pragma once
#include <iostream>
#include <unordered_map>
using namespace std;
#include "Mesh.h"
#include "DefaultBuffer.h"
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
	void CreateBottomAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList);
	void ReleaseScratch();

	Mesh *GetMesh(int _instanceID);
	D3D12_INPUT_ELEMENT_DESC* GetDefaultInputLayout();
	UINT GetDefaultInputLayoutSize();

private:
	vector<Mesh> meshes;
	vector<int> indexInHeap;
	vector<D3D12_INPUT_ELEMENT_DESC> defaultInputLayout;
	unordered_map<int, int> meshIndexTable;
};