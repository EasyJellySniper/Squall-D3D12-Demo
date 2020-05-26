#pragma once
#include "Renderer.h"
using namespace std;
#include <map>

struct QueueRenderer
{
	Renderer* cache;
	int submeshIndex;
};

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

	int AddRenderer(int _instanceID, int _meshInstanceID, int _numOfMaterial, int* _renderQueue);
	void UpdateRendererBound(int _id, float _x, float _y, float _z, float _ex, float _ey, float _ez);
	void SetWorldMatrix(int _id, XMFLOAT4X4 _world);
	void Release();
	vector<shared_ptr<Renderer>> &GetRenderers();
	map<int, vector<QueueRenderer>>& GetQueueRenderers();

private:
	vector<shared_ptr<Renderer>> renderers;
	map<int, vector<QueueRenderer>> queuedRenderers;
};