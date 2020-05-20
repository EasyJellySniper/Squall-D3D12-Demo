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
	XMFLOAT4X4 GetWorld();
	Mesh *GetMesh();
	BoundingBox GetBound();
	bool GetVisible();
	int GetInstanceID();
	D3D12_GPU_VIRTUAL_ADDRESS GetSystemConstantGPU(int _frameIdx);

private:
	SystemConstant currentSysConst;
	unique_ptr<UploadBuffer<SystemConstant>> rendererConstant[FrameCount];
	Mesh *mesh;
	BoundingBox bound;
	bool isVisible;
	int instanceID = -1;

	XMFLOAT4X4 world;
};