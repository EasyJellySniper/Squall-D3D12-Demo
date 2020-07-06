#include "RendererManager.h"
#include "Material.h"
#include "MaterialManager.h"
#include <algorithm>

void RendererManager::Init()
{
	// request memory for common render queue
	queuedRenderers[RenderQueue::Opaque].reserve(OPAQUE_CAPACITY);
	queuedRenderers[RenderQueue::Cutoff].reserve(CUTOFF_CAPACITY);
	queuedRenderers[RenderQueue::Transparent].reserve(TRANSPARENT_CAPACITY);
}

int RendererManager::AddRenderer(int _instanceID, int _meshInstanceID)
{
	int id = -1;
	for (int i = 0; i < (int)renderers.size(); i++)
	{
		if (renderers[i]->GetInstanceID() == _instanceID)
		{
			id = i;
			return id;
		}
	}

	renderers.push_back(std::move(make_shared<Renderer>()));
	id = (int)renderers.size() - 1;
	renderers[id]->Init(_meshInstanceID);
	renderers[id]->SetInstanceID(_instanceID);

	return id;
}

void RendererManager::AddMaterial(int _instanceID, int _matInstanceId, int _renderQueue, int _cullMode, int _srcBlend, int _dstBlend, char* _nativeShader, int _numMacro, char** _macro)
{
	if (_instanceID < 0 || _instanceID >= (int)renderers.size())
	{
		return;
	}

	renderers[_instanceID]->AddMaterial(MaterialManager::Instance().AddMaterial(_matInstanceId, _renderQueue, _cullMode, _srcBlend, _dstBlend, _nativeShader, _numMacro, _macro));
}

void RendererManager::AddToQueueRenderer(Renderer* _renderer, Camera *_camera)
{
	XMFLOAT3 camPos = _camera->GetPosition();
	auto mat = _renderer->GetMaterials();

	for (int i = 0; i < _renderer->GetNumMaterials(); i++)
	{
		QueueRenderer qr;
		qr.cache = _renderer;
		qr.submeshIndex = i;
		
		// calc z distance to camera
		auto bound = _renderer->GetBound();
		float minZ = bound.Center.z - bound.Extents.z;
		float maxZ = bound.Center.z + bound.Extents.z;

		// get min z distance to camera
		qr.zDistanceToCam = min(abs(minZ - camPos.z), abs(maxZ - camPos.z));
		
		queuedRenderers[mat[i]->GetRenderQueue()].push_back(qr);
	}
}

void RendererManager::ClearQueueRenderer()
{
	for (auto& r : queuedRenderers)
	{
		r.second.clear();
	}
}

void RendererManager::UpdateRendererBound(int _id, float _x, float _y, float _z, float _ex, float _ey, float _ez)
{
	if (_id < 0 || _id >= (int)renderers.size())
	{
		return;
	}

	renderers[_id]->UpdateBound(_x, _y, _z, _ex, _ey, _ez);
}

void RendererManager::SetWorldMatrix(int _id, XMFLOAT4X4 _world)
{
	if (_id < 0 || _id >= (int)renderers.size())
	{
		return;
	}

	renderers[_id]->SetWorld(_world);
}

void RendererManager::AddNativeMaterialProp(int _id, int _matId, UINT _byteSize, void* _data)
{
	if (_id < 0 || _id >= (int)renderers.size())
	{
		return;
	}
	renderers[_id]->AddMaterialProp(_matId, _byteSize, _data);
}

void RendererManager::Release()
{
	for (auto&r : renderers)
	{
		r->Release();
		r.reset();
	}

	for (auto& r : queuedRenderers)
	{
		r.second.clear();
	}

	renderers.clear();
	queuedRenderers.clear();
}

void RendererManager::SetNativeRendererActive(int _id, bool _active)
{
	if (_id < 0 || _id >= (int)renderers.size())
	{
		return;
	}
	renderers[_id]->SetActive(_active);
}

void RendererManager::SortWork(Camera* _camera)
{
	ClearQueueRenderer();

	for (int i = 0; i < (int)renderers.size(); i++)
	{
		if (renderers[i]->GetVisible())
		{
			AddToQueueRenderer(renderers[i].get(), _camera);
		}
	}

	for (auto& qr : queuedRenderers)
	{
		if (qr.first <= RenderQueue::OpaqueLast)
		{
			// sort front-to-back
			sort(qr.second.begin(), qr.second.end(), FrontToBackRender);
		}
		else
		{
			// sort back-to-front
			sort(qr.second.begin(), qr.second.end(), BackToFrontRender);
		}
	}
}

vector<shared_ptr<Renderer>>& RendererManager::GetRenderers()
{
	return renderers;
}

map<int, vector<QueueRenderer>>& RendererManager::GetQueueRenderers()
{
	return queuedRenderers;
}
