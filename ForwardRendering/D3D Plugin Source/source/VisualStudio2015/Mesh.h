#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>
#include "DefaultBuffer.h"
using namespace DirectX;
using namespace std;
using namespace Microsoft::WRL;

struct SubMesh
{
	unsigned int IndexCountPerInstance;
	unsigned int StartIndexLocation;
	int BaseVertexLocation;
};

struct MeshData
{
	int subMeshCount;
	SubMesh *submesh;
	void* vertexBuffer;
	unsigned int vertexSizeInBytes;
	unsigned int vertexStrideInBytes;
	void* indexBuffer;
	unsigned int indexSizeInBytes;
	int indexFormat;
};

class Mesh
{
public:
	bool Initialize(int _instanceID, MeshData _mesh);
	void Release();
	void ReleaseScratch();

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();
	SubMesh GetSubMesh(int _index);
	void CreateBottomAccelerationStructure(ID3D12GraphicsCommandList5 *_dxrList);
	ID3D12Resource* GetBottomAS(int _submesh);
	int GetVertexSrv();

private:
	MeshData meshData;
	int instanceID;
	int vertexBufferSrv;
	int indexBufferSrv;

	vector<SubMesh> submeshes;

	unique_ptr<DefaultBuffer> vertexBuffer;
	unique_ptr<DefaultBuffer> indexBuffer;

	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW ibv;

	vector<unique_ptr<DefaultBuffer>> scratchBottom;
	vector<unique_ptr<DefaultBuffer>> bottomLevelAS;
};