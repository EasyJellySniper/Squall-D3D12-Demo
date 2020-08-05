#include "Renderer.h"
#include "GraphicManager.h"

void Renderer::Init(int _meshID)
{
	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		rendererConstant[i] = make_unique<UploadBuffer<ObjectConstant>>(GraphicManager::Instance().GetDevice(), 1, true);
		isDirty[i] = true;
	}

	mesh = MeshManager::Instance().GetMesh(_meshID);
	isVisible = true;
	isShadowVisible = true;
	isActive = true;
}

void Renderer::Release()
{
	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		rendererConstant[i].reset();
	}
	materials.clear();

	mesh = nullptr;
}

void Renderer::UpdateObjectConstant(ObjectConstant _sc, int _frameIdx)
{
	currentObjConst = _sc;
	rendererConstant[_frameIdx]->CopyData(0, currentObjConst);
	isDirty[_frameIdx] = false;
}

void Renderer::UpdateBound(float _cx, float _cy, float _cz, float _ex, float _ey, float _ez)
{
	XMFLOAT3 center = XMFLOAT3(_cx, _cy, _cz);
	XMFLOAT3 extent = XMFLOAT3(_ex, _ey, _ez);
	bound = BoundingBox(center, extent);
}

void Renderer::SetVisible(bool _visible)
{
	isVisible = _visible && isActive;
}

void Renderer::SetShadowVisible(bool _visible)
{
	isShadowVisible = _visible;
}

void Renderer::SetActive(bool _active)
{
	isActive = _active;
}

void Renderer::SetDirty(int _frameIdx)
{
	isDirty[_frameIdx] = true;
}

void Renderer::SetWorld(XMFLOAT4X4 _world)
{
	world = _world;

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		SetDirty(i);
	}
}

void Renderer::SetInstanceID(int _id)
{
	instanceID = _id;
}

void Renderer::AddMaterial(Material* _material)
{
	materials.push_back(_material);
}

void Renderer::AddMaterialProp(int _matId, UINT _byteSize, void* _data)
{
	if (_matId < 0 || _matId >= (int)materials.size())
	{
		return;
	}

	materials[_matId]->AddMaterialConstant(_byteSize, _data);
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

bool Renderer::GetShadowVisible()
{
	return isShadowVisible;
}

bool Renderer::GetActive()
{
	return isActive;
}

bool Renderer::IsDirty(int _frameIdx)
{
	return isDirty[_frameIdx];
}

int Renderer::GetInstanceID()
{
	return instanceID;
}

int Renderer::GetNumMaterials()
{
	return (int)materials.size();
}

const vector<Material*> Renderer::GetMaterials()
{
	return materials;
}

Material* const Renderer::GetMaterial(int _index)
{
	if (_index < 0 || _index >= (int)materials.size())
	{
		return nullptr;
	}

	return materials[_index];
}

D3D12_GPU_VIRTUAL_ADDRESS Renderer::GetObjectConstantGPU(int _frameIdx)
{
	return rendererConstant[_frameIdx]->Resource()->GetGPUVirtualAddress();
}
