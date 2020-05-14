#include "Renderer.h"
#include "GraphicManager.h"

void Renderer::Init(int _meshID)
{
	for (int i = 0; i < FrameCount; i++)
	{
		rendererConstant[i] = make_unique<UploadBuffer<SystemConstant>>(GraphicManager::Instance().GetDevice(), 1, true);
	}

	mesh = MeshManager::Instance().GetMesh(_meshID);
	isVisible = true;
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

void Renderer::UpdateBound(float _cx, float _cy, float _cz, float _ex, float _ey, float _ez)
{
	XMFLOAT3 center = XMFLOAT3(_cx, _cy, _cz);
	XMFLOAT3 extent = XMFLOAT3(_ex, _ey, _ez);
	bound = BoundingBox(center, extent);
}

void Renderer::SetVisible(bool _visible)
{
	isVisible = _visible;
}

Mesh * Renderer::GetMesh()
{
	return mesh;
}

BoundingBox Renderer::GetBound()
{
	return bound;
}

D3D12_GPU_VIRTUAL_ADDRESS Renderer::GetSystemConstantGPU(int _frameIdx)
{
	return rendererConstant[_frameIdx]->Resource()->GetGPUVirtualAddress();
}
