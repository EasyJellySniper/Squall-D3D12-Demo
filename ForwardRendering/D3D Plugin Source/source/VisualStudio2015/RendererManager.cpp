#include "RendererManager.h"

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
	
	// add renderer cache to queue
	for (int i = 0; i < _numOfMaterial; i++)
	{
		QueueRenderer qr;
		qr.cache = renderers[id].get();
		qr.submeshIndex = i;
		queuedRenderers[_renderQueue[i]].push_back(qr);
	}

	return id;
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
