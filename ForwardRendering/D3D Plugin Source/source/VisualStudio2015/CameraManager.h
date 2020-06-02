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
	void SetViewProjMatrix(int _instanceID, XMFLOAT4X4 _view, XMFLOAT4X4 _proj, XMFLOAT4X4 _projCulling, XMFLOAT3 _position);
	void SetViewPortScissorRect(int _instanceID, D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect);
	void Release();
	vector<Camera> &GetCameras();
	Camera* GetCamera(int _instanceID);

private:

	vector<Camera> cameras;
	unordered_map<int, Camera*> cameraLookup;
};