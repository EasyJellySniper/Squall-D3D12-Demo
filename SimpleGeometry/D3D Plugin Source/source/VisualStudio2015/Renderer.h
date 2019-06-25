#pragma once
#include "FrameResource.h"
#include "UploadBuffer.h"
#include "MeshManager.h"

class Renderer
{
public:
	void Init(int _meshID);
	void Release();
	void UpdateSystemConstant(SystemConstant _sc, int _frameIdx);
	Mesh *GetMesh();
	D3D12_GPU_VIRTUAL_ADDRESS GetSystemConstantGPU(int _frameIdx);

private:
	SystemConstant currentSysConst;
	unique_ptr<UploadBuffer<SystemConstant>> rendererConstant[FrameCount];
	Mesh *mesh;
};