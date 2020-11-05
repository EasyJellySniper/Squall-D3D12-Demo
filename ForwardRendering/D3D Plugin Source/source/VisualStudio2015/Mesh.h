#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>
#include "DefaultBuffer.h"
#include "ResourceManager.h"
using namespace DirectX;
using namespace std;
using namespace Microsoft::WRL;

struct SubMesh
{
	unsigned int IndexCountPerInstance;
	unsigned int StartIndexLocation;
	int BaseVertexLocation;
	float padding;
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
	void DrawSubMesh(ID3D12GraphicsCommandList* _cmdList, int _subIndex, int _instanceCount);

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();
	SubMesh GetSubMesh(int _index);
	void CreateBottomAccelerationStructure(ID3D12GraphicsCommandList5 *_dxrList);
	ID3D12Resource* GetBottomAS(int _submesh);
	int GetVertexSrv();
	int GetInstanceID();

private:
	MeshData meshData;
	int instanceID;
	DescriptorHeapData vertexSrv;
	DescriptorHeapData indexSrv;

	vector<SubMesh> submeshes;

	unique_ptr<DefaultBuffer> vertexBuffer;
	unique_ptr<DefaultBuffer> indexBuffer;

	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW ibv;

	vector<unique_ptr<DefaultBuffer>> scratchBottom;
	vector<unique_ptr<DefaultBuffer>> bottomLevelAS;
};