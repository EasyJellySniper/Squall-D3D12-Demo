#include "RendererManager.h"
#include "Material.h"

void RendererManager::Init()
{
	// request memory for common render queue
	queuedRenderers[RenderQueue::Opaque].reserve(OPAQUE_CAPACITY);
	queuedRenderers[RenderQueue::Cutoff].reserve(CUTOFF_CAPACITY);
	queuedRenderers[RenderQueue::Transparent].reserve(TRANSPARENT_CAPACITY);
}

int RendererManager::AddRenderer(int _instanceID, int _meshInstanceID, int _numOfMaterial, int* _renderQueue)
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
	renderers[id]->SetRenderQueue(_numOfMaterial, _renderQueue);
	
	return id;
}

void RendererManager::AddToQueueRenderer(Renderer* _renderer, Camera _camera)
{
	XMFLOAT3 camPos = _camera.GetPosition();
	auto renderQueue = _renderer->GetSubRenderQueue();

	for (int i = 0; i < _renderer->GetNumSubRender(); i++)
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
		
		queuedRenderers[renderQueue[i]].push_back(qr);
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

vector<shared_ptr<Renderer>>& RendererManager::GetRenderers()
{
	return renderers;
}

map<int, vector<QueueRenderer>>& RendererManager::GetQueueRenderers()
{
	return queuedRenderers;
}
