#pragma once
#include "Camera.h"
#include <vector>
#include <unordered_map>
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
	void RemoveCamera(int _instanceID);
	void Release();
	Camera* GetCamera();
	Camera* GetCamera(int _instanceID);

private:

	vector<Camera> cameras;
	unordered_map<int, Camera*> cameraLookup;
};