#pragma once
#include "Camera.h"
#include "Renderer.h"
using namespace std;
#include <map>

struct QueueRenderer
{
	Renderer* cache;
	int submeshIndex;
	float zDistanceToCam;
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

	void Init();
	int AddRenderer(int _instanceID, int _meshInstanceID);
	void AddMaterial(int _instanceID, int _matInstanceId, int _renderQueue, int _cullMode, int _srcBlend, int _dstBlend, char* _nativeShader, int _numMacro, char** _macro);
	void AddToQueueRenderer(Renderer* _renderer, Camera _camera);
	void ClearQueueRenderer();
	void UpdateRendererBound(int _id, float _x, float _y, float _z, float _ex, float _ey, float _ez);
	void SetWorldMatrix(int _id, XMFLOAT4X4 _world);
	void AddNativeMaterialProp(int _id, int _matId, UINT _byteSize, void* _data);
	void Release();

	vector<shared_ptr<Renderer>> &GetRenderers();
	map<int, vector<QueueRenderer>>& GetQueueRenderers();

private:
	static const int OPAQUE_CAPACITY = 5000;
	static const int CUTOFF_CAPACITY = 2500;
	static const int TRANSPARENT_CAPACITY = 500;

	vector<shared_ptr<Renderer>> renderers;
	map<int, vector<QueueRenderer>> queuedRenderers;
};