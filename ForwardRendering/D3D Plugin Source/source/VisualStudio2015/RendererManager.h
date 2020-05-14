#pragma once
#include "Renderer.h"
#include <unordered_map>
using namespace std;

class RendererManager
{
public:
	RendererManager(const RendererManager&) = delete;
	RendererManager(RendererManager&&) = delete;
	RendererManager& operator=(const RendererManager&) = delete;
	RendererManager& operator=(RendererManager&&) = delete;

	static RendererManager& Instance()
	{
		static RendererManager instance;
		return instance;
	}

	RendererManager() {}
	~RendererManager() {}

	bool AddRenderer(int _instanceID, int _meshInstanceID);
	void UpdateRendererBound(int _instanceID, float _x, float _y, float _z, float _ex, float _ey, float _ez);
	void Release();
	unordered_map<int, shared_ptr<Renderer>> &GetRenderers();

private:
	unordered_map<int, shared_ptr<Renderer>> renderers;
};