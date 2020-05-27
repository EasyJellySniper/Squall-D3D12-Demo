#pragma once
#include "FrameResource.h"
#include "UploadBuffer.h"
#include "MeshManager.h"
#include <DirectXCollision.h>
using namespace DirectX;

class Renderer
{
public:
	void Init(int _meshID);
	void Release();
	void UpdateSystemConstant(SystemConstant _sc, int _frameIdx);
	void UpdateBound(float _cx,float _cy, float _cz, float _ex, float _ey, float _ez);
	void SetVisible(bool _visible);
	void SetWorld(XMFLOAT4X4 _world);
	void SetInstanceID(int _id);
	void SetRenderQueue(int _numOfMaterial, int* _renderQueue);
	XMFLOAT4X4 GetWorld();
	Mesh *GetMesh();
	BoundingBox GetBound();
	bool GetVisible();
	int GetInstanceID();
	int GetNumSubRender();
	vector<int> GetSubRenderQueue();
	D3D12_GPU_VIRTUAL_ADDRESS GetSystemConstantGPU(int _frameIdx);

private:
	SystemConstant currentSysConst;
	unique_ptr<UploadBuffer<SystemConstant>> rendererConstant[MAX_FRAME_COUNT];
	Mesh *mesh;
	BoundingBox bound;
	bool isVisible;
	int instanceID = -1;
	vector<int> subRenderQueue;
	int numSubRender = 0;

	XMFLOAT4X4 world;
};