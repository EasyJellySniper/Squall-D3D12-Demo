#pragma once
#include "Camera.h"
#include "Renderer.h"
#include "Light.h"
using namespace std;
#include <map>

struct QueueRenderer
{
	Renderer* cache;
	int submeshIndex;
	int materialID;
	float zDistanceToCam;
};

inline bool FrontToBackRender(QueueRenderer const& i, QueueRenderer const& j)
{
	// sort from low distance to high distance (front to back)
	return i.zDistanceToCam < j.zDistanceToCam;
}

inline bool BackToFrontRender(QueueRenderer const& i, QueueRenderer const& j)
{
	// sort from high distance to low distance (back to front)
	return i.zDistanceToCam > j.zDistanceToCam;
}

inline bool MaterialIdSort(QueueRenderer const& i, QueueRenderer const& j)
{
	return i.materialID < j.materialID;
}

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
	int AddRenderer(int _instanceID, int _meshInstanceID, bool _isDynamic);
	void AddCreatedMaterial(int _instanceID, Material *_mat);
	void UpdateRendererBound(int _id, float _x, float _y, float _z, float _ex, float _ey, float _ez);
	void UploadObjectConstant(Camera* _camera, int _frameIdx, int _threadIndex, int _numThreads);
	void SetWorldMatrix(int _id, XMFLOAT4X4 _world);
	void Release();
	void SetNativeRendererActive(int _id, bool _active);
	void SortWork(Camera* _camera);
	void FrustumCulling(Camera* _camera, int _threadIdx);
	bool ValidRenderer(int _index, vector<QueueRenderer> _renderers);

	vector<shared_ptr<Renderer>> &GetRenderers();
	map<int, vector<QueueRenderer>>& GetQueueRenderers();

private:
	static const int OPAQUE_CAPACITY = 5000;
	static const int CUTOFF_CAPACITY = 2500;
	static const int TRANSPARENT_CAPACITY = 500;

	void ClearQueueRenderer();
	void AddToQueueRenderer(Renderer* _renderer, Camera* _camera);

	vector<shared_ptr<Renderer>> renderers;
	map<int, vector<QueueRenderer>> queuedRenderers;
};