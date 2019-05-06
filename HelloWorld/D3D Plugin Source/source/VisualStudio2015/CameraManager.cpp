#include "CameraManager.h"
#include <fstream>
#include <algorithm> 
using namespace std;

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
	}

	return camInit;
}

void CameraManager::Release()
{
	for (size_t i = 0; i < cameras.size(); i++)
	{
		cameras[i].Release();
	}

	cameras.clear();
}

vector<Camera> CameraManager::GetCameras()
{
	return cameras;
}


