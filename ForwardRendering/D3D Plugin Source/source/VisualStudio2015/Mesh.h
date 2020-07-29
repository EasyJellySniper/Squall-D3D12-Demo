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
	int vertexBufferCount;
	int subMeshCount;
	SubMesh *submesh;
	void** vertexBuffer;
	unsigned int *vertexSizeInBytes;
	unsigned int *vertexStrideInBytes;
	void* indexBuffer;
	unsigned int indexSizeInBytes;
	int indexFormat;
};

class Mesh
{
public:
	bool Initialize(MeshData _mesh);
	void Release();
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();
	SubMesh GetSubMesh(int _index);

private:
	void CreateBottomAccelerationStructure();
	MeshData meshData;

	vector<SubMesh> submeshes;

	vector<ID3D12Resource*> vertexBuffer;
	ID3D12Resource *indexBuffer;

	vector<D3D12_VERTEX_BUFFER_VIEW> vbv;
	D3D12_INDEX_BUFFER_VIEW ibv;

	unique_ptr<DefaultBuffer> scratchBottom;
	unique_ptr<DefaultBuffer> bottomLevelAS;
};