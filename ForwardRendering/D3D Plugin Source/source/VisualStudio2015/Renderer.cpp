#include "Renderer.h"
#include "GraphicManager.h"

void Renderer::Init(int _meshID)
{
	for (int i = 0; i < FrameCount; i++)
	{
		rendererConstant[i] = make_unique<UploadBuffer<SystemConstant>>(GraphicManager::Instance().GetDevice(), 1, true);
	}

	mesh = MeshManager::Instance().GetMesh(_meshID);
}

void Renderer::Release()
{
	for (int i = 0; i < FrameCount; i++)
	{
		rendererConstant[i].reset();
	}

	mesh = nullptr;
}

void Renderer::UpdateSystemConstant(SystemConstant _sc, int _frameIdx)
{
	currentSysConst = _sc;
	rendererConstant[_frameIdx]->CopyData(0, currentSysConst);
}

Mesh * Renderer::GetMesh()
{
	return mesh;
}

D3D12_GPU_VIRTUAL_ADDRESS Renderer::GetSystemConstantGPU(int _frameIdx)
{
	return rendererConstant[_frameIdx]->Resource()->GetGPUVirtualAddress();
}
