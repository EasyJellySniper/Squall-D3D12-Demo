#pragma once
#include "Camera.h"
#include <vector>
using namespace std;

class CameraManager
{
public:
	CameraManager(const CameraManager&) = delete;
	CameraManager(CameraManager&&) = delete;
	CameraManager& operator=(const CameraManager&) = delete;
	CameraManager& operator=(CameraManager&&) = delete;

	static CameraManager& Instance()
	{
		static CameraManager instance;
		return instance;
	}

	CameraManager() {}
	~CameraManager() {}

	bool AddCamera(CameraData _camData);
	void Release();
	vector<Camera> GetCameras();

private:
	vector<Camera> cameras;
};