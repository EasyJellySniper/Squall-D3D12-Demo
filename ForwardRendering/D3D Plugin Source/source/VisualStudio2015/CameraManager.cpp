#include "CameraManager.h"
#include "GraphicManager.h"
#include "MaterialManager.h"
#include <fstream>
#include <algorithm> 
using namespace std;
#include "stdafx.h"

bool SortFunction(Camera &_lhs, Camera &_rhs)
{
	return _lhs.GetCameraData()->camOrder < _rhs.GetCameraData()->camOrder;
}

bool CameraManager::AddCamera(CameraData _camData)
{
	Camera cam;
	bool camInit = cam.Initialize(_camData);

	if (camInit)
	{
		cameras.push_back(cam);
		sort(cameras.begin(), cameras.end(), SortFunction);
		cameraLookup[cam.GetCameraData()->instanceID] = GetCamera(_camData.instanceID);
		MaterialManager::Instance().ResetNativeMaterial(cameraLookup[cam.GetCameraData()->instanceID]);
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
		if (i->GetCameraData()->instanceID == _instanceID)
		{
			i->Release();
			cameras.erase(cameras.begin() + removeIdx);
			break;
		}
	}

	// remove instance lookup
	cameraLookup.erase(_instanceID);
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

Camera* CameraManager::GetCamera()
{
	return &cameras[0];
}

Camera *CameraManager::GetCamera(int _instanceID)
{
	for (size_t i = 0; i < cameras.size(); i++)
	{
		if (cameras[i].GetCameraData()->instanceID == _instanceID)
		{
			return &cameras[i];
		}
	}

	return nullptr;
}


