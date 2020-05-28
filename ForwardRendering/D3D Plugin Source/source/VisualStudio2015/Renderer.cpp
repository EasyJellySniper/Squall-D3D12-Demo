#include "Renderer.h"
#include "GraphicManager.h"

void Renderer::Init(int _meshID)
{
	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		rendererConstant[i] = make_unique<UploadBuffer<SystemConstant>>(GraphicManager::Instance().GetDevice(), 1, true);
	}

	mesh = MeshManager::Instance().GetMesh(_meshID);
	isVisible = true;
}

void Renderer::Release()
{
	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		rendererConstant[i].reset();
	}

	for (size_t i = 0; i < materials.size(); i++)
	{
		materials[i].Release();
	}
	materials.clear();

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

void Renderer::SetWorld(XMFLOAT4X4 _world)
{
	world = _world;
}

void Renderer::SetInstanceID(int _id)
{
	instanceID = _id;
}

void Renderer::AddMaterial(int _renderQueue)
{
	Material m;
	m.SetRenderQueue(_renderQueue);
	materials.push_back(m);
}

void Renderer::AddMaterialProp(int _matId, UINT _byteSize, void* _data)
{
	if (_matId < 0 || _matId >= (int)materials.size())
	{
		return;
	}

	materials[_matId].AddMaterialConstant(_byteSize, _data);
}

XMFLOAT4X4 Renderer::GetWorld()
{
	return world;
}

Mesh * Renderer::GetMesh()
{
	return mesh;
}

BoundingBox Renderer::GetBound()
{
	return bound;
}

bool Renderer::GetVisible()
{
	return isVisible;
}

int Renderer::GetInstanceID()
{
	return instanceID;
}

int Renderer::GetNumMaterials()
{
	return (int)materials.size();
}

const vector<Material> Renderer::GetMaterials()
{
	return materials;
}

D3D12_GPU_VIRTUAL_ADDRESS Renderer::GetSystemConstantGPU(int _frameIdx)
{
	return rendererConstant[_frameIdx]->Resource()->GetGPUVirtualAddress();
}
