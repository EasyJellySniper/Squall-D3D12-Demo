#include "RendererManager.h"

bool RendererManager::AddRenderer(int _instanceID, int _meshInstanceID)
{
	if (renderers.find(_instanceID) == renderers.end())
	{
		renderers[_instanceID] = std::move(make_shared<Renderer>());
		renderers[_instanceID]->Init(_meshInstanceID);
	}

	return true;
}

void RendererManager::UpdateRendererBound(int _instanceID, float _x, float _y, float _z, float _ex, float _ey, float _ez)
{
	if (renderers.find(_instanceID) == renderers.end())
	{
		return;
	}

	renderers[_instanceID]->UpdateBound(_x, _y, _z, _ex, _ey, _ez);
}

void RendererManager::Release()
{
	for (auto&r : renderers)
	{
		r.second->Release();
		r.second.reset();
	}
	renderers.clear();
}

unordered_map<int, shared_ptr<Renderer>>& RendererManager::GetRenderers()
{
	return renderers;
}
