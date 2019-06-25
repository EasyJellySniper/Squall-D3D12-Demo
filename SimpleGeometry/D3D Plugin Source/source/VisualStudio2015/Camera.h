#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>
using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;
#include "MaterialManager.h"

const int MaxRenderTargets = 8;

// camera data
struct CameraData
{
	float *clearColor;
	float camOrder;
	int renderingPath;
	int allowMSAA;
	int instanceID;
	void **renderTarget;
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
	ID3D12Resource *GetMsaaRtvSrc(int _index);
	ID3D12DescriptorHeap *GetRtv();
	ID3D12DescriptorHeap *GetMsaaRtv();
	ID3D12DescriptorHeap *GetDsv();
	void SetViewProj(XMFLOAT4X4 _viewProj);
	void SetViewPortScissorRect(D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect);
	D3D12_VIEWPORT GetViewPort();
	D3D12_RECT GetScissorRect();
	XMFLOAT4X4 GetViewProj();
	Material GetDebugMaterial();

private:
	HRESULT CreateRtvDescriptorHeaps();
	HRESULT CreateDsvDescriptorHeaps();
	void CreateRtv();
	void CreateDsv();
	DXGI_FORMAT GetColorFormat(DXGI_FORMAT _typelessFormat);
	DXGI_FORMAT GetDepthFormat(DXGI_FORMAT _typelessFormat);
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS CheckMsaaQuality(int _sampleCount, DXGI_FORMAT _format);
	bool CreateDebugMaterial();

	CameraData cameraData;

	// render targets
	vector<ID3D12Resource*> renderTarget;
	vector<ComPtr<ID3D12Resource>> msaaTarget;
	ID3D12Resource *depthTarget;
	ComPtr<ID3D12Resource> msaaDepthTarget;
	D3D12_RESOURCE_DESC renderTarrgetDesc[MaxRenderTargets];
	D3D12_RESOURCE_DESC depthTargetDesc;

	D3D12_CLEAR_VALUE optClearColor;
	D3D12_CLEAR_VALUE optClearDepth;
	int numOfRenderTarget;
	int msaaQuality;

	// rtv and dsv handles
	ComPtr<ID3D12DescriptorHeap> rtvHandle;
	ComPtr<ID3D12DescriptorHeap> msaaRtvHandle;
	ComPtr<ID3D12DescriptorHeap> dsvHandle;

	// debug material
	Material debugWireFrame;

	// matrix and view port
	XMFLOAT4X4 viewProj;
	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissorRect;
};