#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <unordered_map>
#include <wrl.h>
#include "Material.h" 
using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

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

enum MaterialType
{
	DebugWireFrame = 0, DepthPrePassOpaque, DepthPrePassCutoff, EndSystemMaterial
};

enum RenderMode
{
	None = 0, WireFrame, Depth
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
	ID3D12Resource *GetMsaaDsvSrc();
	ID3D12DescriptorHeap *GetRtv();
	ID3D12DescriptorHeap *GetMsaaRtv();
	ID3D12DescriptorHeap *GetDsv();
	ID3D12DescriptorHeap *GetMsaaDsv();
	void SetViewProj(XMFLOAT4X4 _view, XMFLOAT4X4 _proj, XMFLOAT4X4 _projCulling, XMFLOAT3 _position);
	void SetViewPortScissorRect(D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect);
	void SetDebugDepth(void *_debugDepth);
	void SetRenderMode(int _mode);

	D3D12_VIEWPORT GetViewPort();
	D3D12_RECT GetScissorRect();
	XMFLOAT4X4 GetViewMatrix();
	XMFLOAT4X4 GetProjMatrix();
	XMFLOAT3 GetPosition();
	Material *GetPipelineMaterial(MaterialType _type);
	int GetNumOfRT();
	D3D12_RESOURCE_DESC* GetColorRTDesc();
	D3D12_RESOURCE_DESC GetDepthDesc();
	int GetMsaaCount();
	int GetMsaaQuailty();
	RenderMode GetRenderMode();
	ID3D12Resource* GetCameraDepth();
	ID3D12Resource* GetDebugDepth();
	bool FrustumTest(BoundingBox _bound);

private:
	static const int MAX_RENDER_TARGETS = 8;

	HRESULT CreateRtvDescriptorHeaps();
	HRESULT CreateDsvDescriptorHeaps();
	void CreateRtv();
	void CreateDsv();
	bool CreatePipelineMaterial();
	DXGI_FORMAT GetColorFormat(DXGI_FORMAT _typelessFormat);
	DXGI_FORMAT GetDepthFormat(DXGI_FORMAT _typelessFormat);
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS CheckMsaaQuality(int _sampleCount, DXGI_FORMAT _format);

	CameraData cameraData;
	RenderMode renderMode = RenderMode::None;

	// render targets
	vector<ID3D12Resource*> renderTarget;
	vector<ComPtr<ID3D12Resource>> msaaTarget;
	ID3D12Resource *depthTarget;
	ID3D12Resource *debugDepth;
	ComPtr<ID3D12Resource> msaaDepthTarget;
	D3D12_RESOURCE_DESC renderTarrgetDesc[MAX_RENDER_TARGETS];
	D3D12_RESOURCE_DESC depthTargetDesc;

	D3D12_CLEAR_VALUE optClearColor;
	D3D12_CLEAR_VALUE optClearDepth;
	int numOfRenderTarget;
	int msaaQuality;

	// rtv and dsv handles
	ComPtr<ID3D12DescriptorHeap> rtvHandle;
	ComPtr<ID3D12DescriptorHeap> msaaRtvHandle;
	ComPtr<ID3D12DescriptorHeap> dsvHandle;
	ComPtr<ID3D12DescriptorHeap> msaaDsvHandle;

	// material
	Material pipelineMaterials[MaterialType::EndSystemMaterial];

	// matrix and view port
	XMFLOAT4X4 viewMatrix;
	XMFLOAT4X4 projMatrix;
	XMFLOAT3 position;
	BoundingFrustum camFrustum;

	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissorRect;
};