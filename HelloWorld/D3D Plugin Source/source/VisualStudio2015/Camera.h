#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

const int MaxRenderTargets = 8;

// camera data
struct CameraData
{
	float clearColor[4];
	float camOrder;
	int renderingPath;
	int allowHDR;
	int allowMSAA;
	void *renderTarget0;
	void *renderTarget1;
	void *renderTarget2;
	void *renderTarget3;
	void *renderTarget4;
	void *renderTarget5;
	void *renderTarget6;
	void *renderTarget7;
	void *depthTarget;
};

class Camera
{
public:
	Camera() {};
	~Camera() {};

	bool Initialize(CameraData _cameraData);
	void Release();
	CameraData GetCameraData();
	ID3D12Resource *GetRtvSrc(int _index);
	ID3D12Resource *GetDsvSrc();
	ID3D12DescriptorHeap *GetRtv();
	ID3D12DescriptorHeap *GetDsv();

private:
	HRESULT CreateRtvDescriptorHeaps();
	HRESULT CreateDsvDescriptorHeaps();
	void CreateRtv();
	void CreateDsv();

	CameraData cameraData;

	ID3D12Resource *renderTarget[MaxRenderTargets];
	ID3D12Resource *depthTarget;

	// rtv and dsv handles
	ComPtr<ID3D12DescriptorHeap> rtvHandle;
	ComPtr<ID3D12DescriptorHeap> dsvHandle;
};