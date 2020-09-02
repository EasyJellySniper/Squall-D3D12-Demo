#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <unordered_map>
#include <wrl.h>
#include "Material.h" 
#include "Shader.h"
#include "Texture.h"
#include "DefaultBuffer.h"
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

struct RenderTargetData
{
	int numRT;
	DXGI_FORMAT* colorDesc;
	DXGI_FORMAT depthDesc;
	int msaaCount;
	int msaaQuality;
};

enum MaterialType
{
	DebugWireFrame = 0, DepthPrePassOpaque, DepthPrePassCutoff, EndSystemMaterial
};

enum RenderMode
{
	None = 0, WireFrame, Depth, ForwardPass
};

enum RenderBufferUsage
{
	Color = 0, TransparentDepth, Normal
};

class Camera
{
public:
	Camera() {};
	~Camera() {};

	bool Initialize(CameraData _cameraData);
	void Release();
	void ClearCamera(ID3D12GraphicsCommandList* _cmdList, bool _clearDepth = true);
	void ResolveDepthNormalBuffer(ID3D12GraphicsCommandList* _cmdList, int _frameIdx);
	void ResolveColorBuffer(ID3D12GraphicsCommandList* _cmdList);

	CameraData *GetCameraData();
	ID3D12Resource *GetRtvSrc();
	ID3D12Resource* GetCameraDepth();
	ID3D12Resource* GetTransparentDepth();
	ID3D12Resource *GetMsaaRtvSrc();
	ID3D12Resource *GetMsaaDsvSrc();
	ID3D12Resource* GetNormalSrc();
	D3D12_CPU_DESCRIPTOR_HANDLE GetRtv();
	D3D12_CPU_DESCRIPTOR_HANDLE GetMsaaRtv();
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsv();
	D3D12_CPU_DESCRIPTOR_HANDLE GetTransDsv();
	D3D12_CPU_DESCRIPTOR_HANDLE GetMsaaDsv();
	D3D12_GPU_DESCRIPTOR_HANDLE GetMsaaSrv();
	D3D12_CPU_DESCRIPTOR_HANDLE GetNormalRtv();

	void SetViewProj(XMFLOAT4X4 _view, XMFLOAT4X4 _proj, XMFLOAT4X4 _projCulling, XMFLOAT4X4 _invView, XMFLOAT4X4 _invProj, XMFLOAT3 _position, float _far, float _near);
	void SetViewPortScissorRect(D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect);
	void SetRenderMode(int _mode);

	D3D12_VIEWPORT GetViewPort();
	D3D12_RECT GetScissorRect();
	XMFLOAT4X4 GetView();
	XMFLOAT4X4 GetProj();
	XMFLOAT4X4 GetInvView();
	XMFLOAT4X4 GetInvProj();
	XMFLOAT3 GetPosition();
	float GetFarZ();
	float GetNearZ();
	Material *GetPipelineMaterial(MaterialType _type, CullMode _cullMode);
	Material* GetResolveDepthMaterial();
	RenderMode GetRenderMode();
	bool FrustumTest(BoundingBox _bound);
	Shader* GetFallbackShader();
	RenderTargetData GetRenderTargetData();
	void FillSystemConstant(SystemConstant& _sc);

private:
	static const int MAX_RENDER_TARGETS = 8;
	void InitColorBuffer();
	void InitDepthBuffer();
	void InitTransparentDepth();
	void InitNormalBuffer();
	bool CreatePipelineMaterial();
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS CheckMsaaQuality(int _sampleCount, DXGI_FORMAT _format);

	CameraData cameraData;
	RenderMode renderMode = RenderMode::None;

	// render targets
	vector<shared_ptr<DefaultBuffer>> msaaTarget;
	shared_ptr<DefaultBuffer> msaaDepthTarget;

	ID3D12Resource* renderTarget[MAX_RENDER_TARGETS];
	ID3D12Resource* depthTarget;
	int opaqueDepthSrv;
	int transDepthSrv;
	int msaaDepthSrv;
	int colorBufferSrv;
	int normalBufferSrv;

	// rt desc cache
	DXGI_FORMAT renderTargetDesc[MAX_RENDER_TARGETS];
	DXGI_FORMAT depthTargetDesc;
	RenderTargetData rtData;

	// render texture
	shared_ptr<Texture> cameraRT;
	shared_ptr<Texture> cameraRTMsaa;
	shared_ptr<Texture> transparentDepth;
	shared_ptr<Texture> normalRT;

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
	XMFLOAT4X4 invViewMatrix;
	XMFLOAT4X4 invProjMatrix;
	XMFLOAT3 position;
	float farZ;
	float nearZ;
	BoundingFrustum camFrustum;

	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissorRect;
};