#include "CameraManager.h"
#include "GraphicManager.h"
#include <fstream>
#include <algorithm> 
using namespace std;
#include "stdafx.h"

bool SortFunction(Camera &_lhs, Camera &_rhs)
{
	return _lhs.GetCameraData().camOrder < _rhs.GetCameraData().camOrder;
}

bool CameraManager::AddCamera(CameraData _camData)
{
	Camera cam;
	bool camInit = cam.Initialize(_camData);

	if (camInit)
	{
		cameras.push_back(cam);
		sort(cameras.begin(), cameras.end(), SortFunction);
		cameraLookup[cam.GetCameraData().instanceID] = GetCamera(_camData.instanceID);
	}

	return camInit;
}

void CameraManager::RemoveCamera(int _instanceID)
{
	// wait gpu job finish
	GraphicManager::Instance().WaitForRenderThread();
	GraphicManager::Instance().WaitForGPU();
	int removeIdx = -1;
	for (auto i = cameras.begin(); i != cameras.end(); i++)
	{
		removeIdx++;
		if (i->GetCameraData().instanceID == _instanceID)
		{
			i->Release();
			cameras.erase(cameras.begin() + removeIdx);
			break;
		}
	}

	// remove instance lookup
	cameraLookup.erase(_instanceID);
}

void CameraManager::SetViewProjMatrix(int _instanceID, XMFLOAT4X4 _view, XMFLOAT4X4 _proj, XMFLOAT4X4 _projCulling)
{
	// find camera and set
	if (cameraLookup.find(_instanceID) != cameraLookup.end())
	{
		cameraLookup[_instanceID]->SetViewProj(_view, _proj, _projCulling);
	}
}

void CameraManager::SetViewPortScissorRect(int _instanceID, D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect)
{
	if (cameraLookup.find(_instanceID) != cameraLookup.end())
	{
		cameraLookup[_instanceID]->SetViewPortScissorRect(_viewPort, _scissorRect);
	}
}

void CameraManager::Release()
{
	for (size_t i = 0; i < cameras.size(); i++)
	{
		cameras[i].Release();
	}

	cameras.clear();
	cameraLookup.clear();
}

vector<Camera> &CameraManager::GetCameras()
{
	return cameras;
}

Camera *CameraManager::GetCamera(int _instanceID)
{
	for (size_t i = 0; i < cameras.size(); i++)
	{
		if (cameras[i].GetCameraData().instanceID == _instanceID)
		{
			return &cameras[i];
		}
	}

	return nullptr;
}


