#include "RendererManager.h"
#include "Material.h"
#include "MaterialManager.h"
#include "GraphicManager.h"
#include <algorithm>

void RendererManager::Init()
{
	// request memory for common render queue
	queuedRenderers[RenderQueue::Opaque].reserve(OPAQUE_CAPACITY);
	queuedRenderers[RenderQueue::Cutoff].reserve(CUTOFF_CAPACITY);
	queuedRenderers[RenderQueue::Transparent].reserve(TRANSPARENT_CAPACITY);
}

int RendererManager::AddRenderer(int _instanceID, int _meshInstanceID, bool _isDynamic)
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
	renderers[id]->Init(_meshInstanceID, _isDynamic);
	renderers[id]->SetInstanceID(_instanceID);

	return id;
}

void RendererManager::AddCreatedMaterial(int _instanceID, Material *_mat)
{
	if (_instanceID < 0 || _instanceID >= (int)renderers.size())
	{
		return;
	}

	renderers[_instanceID]->AddMaterial(_mat);
}

void RendererManager::InitInstanceRendering()
{
	// crate instance renderer & count capacity
	for (auto &r : renderers)
	{
		auto mats = r->GetMaterials();
		for (int i = 0; i < r->GetNumMaterials(); i++)
		{
			InstanceRenderer ir;
			ir.cache = r.get();
			ir.materialID = mats[i]->GetInstanceID();
			ir.submeshIndex = i;

			int queue = mats[i]->GetRenderQueue();
			int idx = FindInstanceRenderer(queue, ir);

			if (idx == -1)
			{
				// init and add to list
				instanceRenderers[queue].push_back(ir);
				idx = (int)instanceRenderers[queue].size() - 1;
			}
			instanceRenderers[queue][idx].CountCapacity();
		}
	}

	// actually create instance upload buffer
	for (auto& r : instanceRenderers)
	{
		for (auto& ir : r.second)
		{
			ir.CreateInstanceData();
		}
	}
}

void RendererManager::AddToQueueRenderer(Renderer* _renderer, Camera *_camera)
{
	XMFLOAT3 camPos = _camera->GetPosition();
	auto mat = _renderer->GetMaterials();

	// calc z distance to camera
	auto bound = _renderer->GetBound();
	float minZ = bound.Center.z - bound.Extents.z;
	float maxZ = bound.Center.z + bound.Extents.z;
	float zDist = min(abs(minZ - camPos.z), abs(maxZ - camPos.z));

	for (int i = 0; i < _renderer->GetNumMaterials(); i++)
	{
		// add transparent object to queue renderer only
		if (_renderer->GetMaterial(i)->GetRenderQueue() <= RenderQueue::OpaqueLast)
		{
			continue;
		}

		QueueRenderer qr;
		qr.cache = _renderer;
		qr.submeshIndex = i;
		qr.materialID = _renderer->GetMaterial(i)->GetInstanceID();

		// get min z distance to camera
		qr.zDistanceToCam = zDist;
		
		queuedRenderers[mat[i]->GetRenderQueue()].push_back(qr);
	}
}

void RendererManager::AddToInstanceRenderer(Renderer* _renderer, Camera *_camera)
{
	XMFLOAT3 camPos = _camera->GetPosition();
	auto mats = _renderer->GetMaterials();

	// calc z distance to camera
	auto bound = _renderer->GetBound();
	float minZ = bound.Center.z - bound.Extents.z;
	float maxZ = bound.Center.z + bound.Extents.z;
	float zDist = min(abs(minZ - camPos.z), abs(maxZ - camPos.z));

	for (int i = 0; i < _renderer->GetNumMaterials(); i++)
	{
		InstanceRenderer ir;
		ir.cache = _renderer;
		ir.materialID = mats[i]->GetInstanceID();
		ir.submeshIndex = i;

		int queue = mats[i]->GetRenderQueue();
		int idx = FindInstanceRenderer(queue, ir);

		if (idx != -1)
		{
			// add instance data
			SqInstanceData sid;
			sid.world = _renderer->GetWorld();
			instanceRenderers[queue][idx].AddInstanceData(sid, zDist);
		}
	}
}

int RendererManager::FindInstanceRenderer(int _queue, InstanceRenderer _ir)
{
	for (int i = 0; i < (int)instanceRenderers[_queue].size(); i++)
	{
		if (_ir == instanceRenderers[_queue][i])
		{
			return i;
		}
	}

	return -1;
}

void RendererManager::ClearQueueRenderer()
{
	for (auto& r : queuedRenderers)
	{
		r.second.clear();
	}
}

void RendererManager::ClearInstanceRendererData()
{
	for (auto& r : instanceRenderers)
	{
		for (auto& ir : r.second)
		{
			ir.ClearInstanceData();
		}
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

void RendererManager::UploadObjectConstant(int _frameIdx, int _threadIndex, int _numThreads)
{
	auto renderers = RendererManager::Instance().GetRenderers();

	// split thread group
	int count = (int)renderers.size() / _numThreads + 1;
	int start = _threadIndex * count;

	// update renderer constant
	for (int i = start; i <= start + count; i++)
	{
		if (i >= (int)renderers.size())
		{
			continue;
		}

		auto r = renderers[i];
		if (!r->GetVisible())
		{
			continue;
		}

		if (!r->IsDirty(_frameIdx))
		{
			continue;
		}

		Mesh* m = r->GetMesh();
		if (m == nullptr)
		{
			continue;
		}

		XMFLOAT4X4 world = r->GetWorld();

		ObjectConstant sc;
		sc.sqMatrixWorld = world;
		r->UpdateObjectConstant(sc, _frameIdx);
	}
}

void RendererManager::UploadInstanceData(int _frameIdx, int _threadIndex, int _numThreads)
{
	for (auto& r : instanceRenderers)
	{
		// split thread group
		int count = (int)r.second.size() / _numThreads + 1;
		int start = _threadIndex * count;

		for (int i = start; i <= start + count; i++)
		{
			if (i >= (int)r.second.size())
			{
				continue;
			}

			auto ir = r.second[i];
			ir.UploadInstanceData(_frameIdx);
		}
	}
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

	for (auto& r : instanceRenderers)
	{
		for (auto& ir : r.second)
		{
			ir.Release();
		}
	}

	renderers.clear();
	queuedRenderers.clear();
	instanceRenderers.clear();
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
	ClearInstanceRendererData();

	for (int i = 0; i < (int)renderers.size(); i++)
	{
		if (renderers[i]->GetVisible())
		{
			AddToQueueRenderer(renderers[i].get(), _camera);
			AddToInstanceRenderer(renderers[i].get(), _camera);
		}
	}

	// sort queue renderers
	for (auto& qr : queuedRenderers)
	{
		if (qr.first > RenderQueue::OpaqueLast)
		{
			// sort back-to-front for transparent obj
			sort(qr.second.begin(), qr.second.end(), BackToFrontRender);
		}
	}

	// sort instance renderers
	for (auto& ir : instanceRenderers)
	{
		for (auto& iir : ir.second)
		{
			iir.FinishCollectInstance();
		}

		if (ir.first <= RenderQueue::OpaqueLast)
		{
			// sort from front to back for instance group
			sort(ir.second.begin(), ir.second.end(), FrontToBackInstance);
		}
	}
}

void RendererManager::FrustumCulling(Camera* _camera, int _threadIdx)
{
	auto numWorkerThreads = GraphicManager::Instance().GetThreadCount() - 1;
	int count = (int)renderers.size() / numWorkerThreads + 1;
	int start = _threadIdx * count;

	for (int i = start; i <= start + count; i++)
	{
		if (i >= (int)renderers.size())
		{
			continue;
		}

		bool isVisible = _camera->FrustumTest(renderers[i]->GetBound());
		renderers[i]->SetVisible(isVisible);
	}
}

bool RendererManager::ValidRenderer(int _index, vector<QueueRenderer>& _renderers)
{
	if (_index >= (int)_renderers.size())
	{
		return false;
	}

	auto const r = _renderers[_index];
	if (!r.cache->GetVisible())
	{
		return false;
	}

	Mesh* m = r.cache->GetMesh();
	if (m == nullptr)
	{
		return false;
	}

	return true;
}

bool RendererManager::ValidRenderer(int _index, vector<InstanceRenderer>& _renderers)
{
	if (_index >= (int)_renderers.size())
	{
		return false;
	}

	auto const r = _renderers[_index];
	Mesh* m = r.cache->GetMesh();
	if (m == nullptr)
	{
		return false;
	}

	return true;
}

vector<shared_ptr<Renderer>>& RendererManager::GetRenderers()
{
	return renderers;
}

map<int, vector<QueueRenderer>>& RendererManager::GetQueueRenderers()
{
	return queuedRenderers;
}

map<int, vector<InstanceRenderer>>& RendererManager::GetInstanceRenderers()
{
	return instanceRenderers;
}
