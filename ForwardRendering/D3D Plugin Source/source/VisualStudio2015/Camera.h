#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <unordered_map>
#include <wrl.h>
#include "Material.h" 
#include "Shader.h"
#include "RenderTexture.h"
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
	None = 0, WireFrame, Depth, ForwardPass
};

class Camera
{
public:
	Camera() {};
	~Camera() {};

	bool Initialize(CameraData _cameraData);
	void Release();

	CameraData *GetCameraData();
	ID3D12Resource *GetRtvSrc(int _index);
	ID3D12Resource* GetCameraDepth();
	ID3D12Resource* GetDebugDepth();
	ID3D12Resource *GetMsaaRtvSrc(int _index);
	ID3D12Resource *GetMsaaDsvSrc();
	D3D12_CPU_DESCRIPTOR_HANDLE GetRtv(int _index);
	D3D12_CPU_DESCRIPTOR_HANDLE GetMsaaRtv(int _index);
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsv();
	D3D12_CPU_DESCRIPTOR_HANDLE GetMsaaDsv();
	ID3D12DescriptorHeap* GetMsaaSrv(int _index);
	void SetViewProj(XMFLOAT4X4 _view, XMFLOAT4X4 _proj, XMFLOAT4X4 _projCulling, XMFLOAT3 _position);
	void SetViewPortScissorRect(D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect);
	void SetRenderMode(int _mode);
	void CopyDepth(void *_dest);

	D3D12_VIEWPORT GetViewPort();
	D3D12_RECT GetScissorRect();
	XMFLOAT4X4 GetViewMatrix();
	XMFLOAT4X4 GetProjMatrix();
	XMFLOAT3 GetPosition();
	Material *GetPipelineMaterial(MaterialType _type, CullMode _cullMode);
	Material* GetPostMaterial();
	int GetNumOfRT();
	D3D12_RESOURCE_DESC* GetColorRTDesc();
	D3D12_RESOURCE_DESC GetDepthDesc();
	int GetMsaaCount();
	int GetMsaaQuailty();
	RenderMode GetRenderMode();
	bool FrustumTest(BoundingBox _bound);
	Shader* GetFallbackShader();

private:
	static const int MAX_RENDER_TARGETS = 8;
	bool CreatePipelineMaterial();
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS CheckMsaaQuality(int _sampleCount, DXGI_FORMAT _format);

	CameraData cameraData;
	RenderMode renderMode = RenderMode::None;

	// render targets
	vector<ComPtr<ID3D12Resource>> msaaTarget;
	ComPtr<ID3D12Resource> msaaDepthTarget;
	ID3D12Resource* debugDepth;

	// rt desc cache
	D3D12_RESOURCE_DESC renderTarrgetDesc[MAX_RENDER_TARGETS];
	D3D12_RESOURCE_DESC depthTargetDesc;

	// render texture
	shared_ptr<RenderTexture> cameraRT[MAX_RENDER_TARGETS];
	shared_ptr<RenderTexture> cameraRTMsaa[MAX_RENDER_TARGETS];

	D3D12_CLEAR_VALUE optClearColor;
	D3D12_CLEAR_VALUE optClearDepth;
	int numOfRenderTarget;
	int msaaQuality;

	// material
	unordered_map<int, vector<Material>> pipelineMaterials;
	Material resolveDepthMaterial;
	Shader* wireFrameDebug;

	// matrix and view port
	XMFLOAT4X4 viewMatrix;
	XMFLOAT4X4 projMatrix;
	XMFLOAT3 position;
	BoundingFrustum camFrustum;

	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissorRect;
};