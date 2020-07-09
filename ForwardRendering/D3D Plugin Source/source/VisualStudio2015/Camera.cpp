#include "Camera.h"
#include "stdafx.h"
#include "GraphicManager.h"
#include "d3dx12.h"
#include "MeshManager.h"
#include "ShaderManager.h"
#include "MaterialManager.h"

bool Camera::Initialize(CameraData _cameraData)
{
	cameraData = _cameraData;
	numOfRenderTarget = 0;
	msaaQuality = 0;

	vector<ID3D12Resource*> renderTarget;
	ID3D12Resource* depthTarget;

	for (int i = 0; i < MAX_RENDER_TARGETS; i++)
	{
		renderTarget.push_back((ID3D12Resource*)cameraData.renderTarget[i]);
	}

	for (int i = 0; i < MAX_RENDER_TARGETS; i++)
	{
		// at least need 1 target
		if (renderTarget[0] == nullptr)
		{
			return false;
		}

		if (renderTarget[i] == nullptr)
		{
			LogMessage(L"[SqGraphic Error] SqCamera: Render Target " + to_wstring(i) + L" pointer is null.");
			continue;
		}

		numOfRenderTarget++;

		// get target desc here
		D3D12_RESOURCE_DESC srcDesc = renderTarget[i]->GetDesc();
		renderTarrgetDesc[i] = srcDesc;
		renderTarrgetDesc[i].Format = GetColorFormatFromTypeless(srcDesc.Format);

		// create msaa target if necessary
		if (cameraData.allowMSAA > 1)
		{
			HRESULT hr = S_OK;

			ComPtr<ID3D12Resource> aaTarget;
			srcDesc.SampleDesc.Count = cameraData.allowMSAA;

			// check aa quality
			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS aaData;
			aaData = CheckMsaaQuality(cameraData.allowMSAA, srcDesc.Format);
			srcDesc.SampleDesc.Quality = max(aaData.NumQualityLevels - 1, 0);
			msaaQuality = max(aaData.NumQualityLevels - 1, 0);

			for (int j = 0; j < 4; j++)
			{
				optClearColor.Color[j] = cameraData.clearColor[j];
			}
			optClearColor.Format = renderTarrgetDesc[i].Format;

			LogIfFailed(GraphicManager::Instance().GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
				, D3D12_HEAP_FLAG_NONE
				, &srcDesc
				, D3D12_RESOURCE_STATE_COMMON
				, &optClearColor
				, IID_PPV_ARGS(&aaTarget)), hr);

			if (SUCCEEDED(hr))
			{
				msaaTarget.push_back(aaTarget);
			}
			else
			{
				LogMessage(L"[SqGraphic Error] SqCamera: MSAA Target creation failed.");
				LogMessage((L"Format: " + to_wstring(renderTarrgetDesc[i].Format)).c_str());
				return false;
			}
		}
	}

	if (cameraData.depthTarget == nullptr)
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Depth Target pointer is null.");
		return false;
	}
	depthTarget = (ID3D12Resource*)cameraData.depthTarget;

	// create aa depth target if necessary
	if (cameraData.allowMSAA > 1)
	{
		D3D12_RESOURCE_DESC srcDesc = depthTarget->GetDesc();
		srcDesc.SampleDesc.Count = cameraData.allowMSAA;

		// check aa quality
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS aaData;
		aaData = CheckMsaaQuality(cameraData.allowMSAA, GetDepthFormatFromTypeless(srcDesc.Format));
		srcDesc.SampleDesc.Quality = max(aaData.NumQualityLevels - 1, 0);

		optClearDepth.DepthStencil.Depth = 0.0f;
		optClearDepth.DepthStencil.Stencil = 0;
		optClearDepth.Format = GetDepthFormatFromTypeless(srcDesc.Format);

		HRESULT hr = S_OK;
		LogIfFailed(GraphicManager::Instance().GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
			, D3D12_HEAP_FLAG_NONE
			, &srcDesc
			, D3D12_RESOURCE_STATE_COMMON
			, &optClearDepth
			, IID_PPV_ARGS(&msaaDepthTarget)), hr);

		if (FAILED(hr))
		{
			LogMessage(L"[SqGraphic Error] SqCamera: MSAA Depth creation failed.");
			return false;
		}
	}

	for (int i = 0; i < numOfRenderTarget; i++)
	{
		cameraRT[i] = make_shared<RenderTexture>();
		cameraRTMsaa[i] = make_shared<RenderTexture>();

		cameraRT[i]->InitRTV(renderTarget[i], renderTarrgetDesc[i].Format);
		if (cameraData.allowMSAA > 1)
		{
			cameraRTMsaa[i]->InitRTV(msaaTarget[i].Get(), renderTarrgetDesc[i].Format, true);
		}
	}

	// create depth, only 1st need depth
	auto depthDesc = depthTarget->GetDesc();
	depthTargetDesc = depthDesc;
	depthTargetDesc.Format = GetDepthFormatFromTypeless(depthDesc.Format);

	cameraRT[0]->InitDSV(depthTarget, depthTargetDesc.Format);
	if (cameraData.allowMSAA > 1)
	{
		cameraRTMsaa[0]->InitDSV(msaaDepthTarget.Get(), depthTargetDesc.Format, true);
		cameraRTMsaa[0]->InitSRV(msaaDepthTarget.Get(), GetShaderFormatFromTypeless(depthDesc.Format), true);
	}

	renderTarget.clear();

	if (!CreatePipelineMaterial())
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Create Debug Material failed.");
		return false;
	}

	return true;
}

void Camera::Release()
{
	for (size_t i = 0; i < msaaTarget.size(); i++)
	{
		msaaTarget[i].Reset();
	}
	msaaDepthTarget.Reset();
	msaaTarget.clear();

	for (int i = 0; i < numOfRenderTarget; i++)
	{
		cameraRT[i]->Release();
		cameraRTMsaa[i]->Release();
		cameraRT[i].reset();
		cameraRTMsaa[i].reset();
	}

	for (auto& m : pipelineMaterials)
	{
		for (int i = 0; i < CullMode::NumCullMode; i++)
		{
			m.second[i].Release();
		}
		m.second.clear();
	}
	pipelineMaterials.clear();
	resolveDepthMaterial.Release();
}

CameraData *Camera::GetCameraData()
{
	return &cameraData;
}

ID3D12Resource * Camera::GetRtvSrc(int _index)
{
	return cameraRT[_index]->GetRtvSrc();
}

ID3D12Resource* Camera::GetDebugDepth()
{
	return debugDepth;
}

ID3D12Resource * Camera::GetMsaaRtvSrc(int _index)
{
	return msaaTarget[_index].Get();
}

ID3D12Resource * Camera::GetMsaaDsvSrc()
{
	return msaaDepthTarget.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE Camera::GetRtv(int _index)
{
	return cameraRT[_index]->GetRtvCPU();
}

D3D12_CPU_DESCRIPTOR_HANDLE Camera::GetMsaaRtv(int _index)
{
	return cameraRTMsaa[_index]->GetRtvCPU();
}

D3D12_CPU_DESCRIPTOR_HANDLE Camera::GetDsv()
{
	return cameraRT[0]->GetDsvCPU();
}

D3D12_CPU_DESCRIPTOR_HANDLE Camera::GetMsaaDsv()
{
	return cameraRTMsaa[0]->GetDsvCPU();
}

ID3D12DescriptorHeap* Camera::GetMsaaSrv(int _index)
{
	return cameraRTMsaa[_index]->GetSrv();
}

void Camera::SetViewProj(XMFLOAT4X4 _view, XMFLOAT4X4 _proj, XMFLOAT4X4 _projCulling, XMFLOAT3 _position)
{
	// data from unity is column major, while d3d matrix use row major
	viewMatrix = _view;
	projMatrix = _proj;
	position = _position;

	// fix view matrix for culling
	XMFLOAT4X4 _viewFix = _view;
	_viewFix._11 *= -1;
	_viewFix._31 *= -1;
	_viewFix._41 *= -1;

	// update inverse view for culling
	XMMATRIX view = XMLoadFloat4x4(&_viewFix);
	XMMATRIX invViewMatrix = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	// update bounding frustum and convert to world space
	BoundingFrustum frustum;
	BoundingFrustum::CreateFromMatrix(frustum, XMLoadFloat4x4(&_projCulling));
	frustum.Transform(camFrustum, invViewMatrix);
}

void Camera::SetViewPortScissorRect(D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect)
{
	viewPort = _viewPort;
	scissorRect = _scissorRect;
}

void Camera::SetRenderMode(int _mode)
{
	renderMode = (RenderMode)_mode;
}

void Camera::CopyDepth(void* _dest)
{
	debugDepth = (ID3D12Resource*)_dest;
}

D3D12_VIEWPORT Camera::GetViewPort()
{
	return viewPort;
}

D3D12_RECT Camera::GetScissorRect()
{
	return scissorRect;
}

XMFLOAT4X4 Camera::GetViewMatrix()
{
	return viewMatrix;
}

XMFLOAT4X4 Camera::GetProjMatrix()
{
	return projMatrix;
}

XMFLOAT3 Camera::GetPosition()
{
	return position;
}

Material *Camera::GetPipelineMaterial(MaterialType _type, CullMode _cullMode)
{
	return &pipelineMaterials[_type][_cullMode];
}

Material* Camera::GetPostMaterial()
{
	return &resolveDepthMaterial;
}

int Camera::GetNumOfRT()
{
	return numOfRenderTarget;
}

D3D12_RESOURCE_DESC* Camera::GetColorRTDesc()
{
	return renderTarrgetDesc;
}

D3D12_RESOURCE_DESC Camera::GetDepthDesc()
{
	return depthTargetDesc;
}

int Camera::GetMsaaCount()
{
	return cameraData.allowMSAA;
}

int Camera::GetMsaaQuailty()
{
	return msaaQuality;
}

RenderMode Camera::GetRenderMode()
{
	return renderMode;
}

ID3D12Resource* Camera::GetCameraDepth()
{
	return cameraRT[0]->GetDsvSrc();
}

bool Camera::FrustumTest(BoundingBox _bound)
{
	return (camFrustum.Contains(_bound) != DirectX::DISJOINT);
}

Shader* Camera::GetFallbackShader()
{
	return wireFrameDebug;
}

bool Camera::CreatePipelineMaterial()
{
	// init vector
	for (int i = 0; i < MaterialType::EndSystemMaterial; i++)
	{
		pipelineMaterials[i].resize(CullMode::NumCullMode);
	}

	// create debug wire frame material
	Shader* wireFrameShader = ShaderManager::Instance().CompileShader(L"DebugWireFrame.hlsl");
	if (wireFrameShader != nullptr)
	{
		pipelineMaterials[MaterialType::DebugWireFrame][CullMode::Off] = MaterialManager::Instance().CreateMaterialFromShader(wireFrameShader, this, D3D12_FILL_MODE_WIREFRAME, D3D12_CULL_MODE_NONE);
		wireFrameDebug = wireFrameShader;
	}

	// create depth prepass material
	D3D_SHADER_MACRO depthMacro[] = { "_CUTOFF_ON","1",NULL,NULL };
	Shader* depthPrePassOpaque = ShaderManager::Instance().CompileShader(L"DepthPrePass.hlsl");
	Shader* depthPrePassCutoff = ShaderManager::Instance().CompileShader(L"DepthPrePass.hlsl", depthMacro);
	if (depthPrePassOpaque != nullptr)
	{
		for (int i = 0; i < CullMode::NumCullMode; i++)
		{
			pipelineMaterials[MaterialType::DepthPrePassOpaque][i] = MaterialManager::Instance().CreateMaterialFromShader(depthPrePassOpaque, this, D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(i + 1));
		}
	}

	if (depthPrePassCutoff != nullptr)
	{
		for (int i = 0; i < CullMode::NumCullMode; i++)
		{
			pipelineMaterials[MaterialType::DepthPrePassCutoff][i] = MaterialManager::Instance().CreateMaterialFromShader(depthPrePassCutoff, this, D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(i + 1));
		}
	}

	Shader* resolveDepth = ShaderManager::Instance().CompileShader(L"ResolveDepth.hlsl", nullptr, true);
	if (resolveDepth != nullptr)
	{
		resolveDepthMaterial = MaterialManager::Instance().CreateMaterialPost(resolveDepth, this, true);

		struct RDConstants
		{
			UINT _msaaCount;
			XMFLOAT3 _padding;
		};
		static RDConstants rConstants;
		rConstants._msaaCount = GetMsaaCount();

		resolveDepthMaterial.AddMaterialConstant(sizeof(RDConstants), &rConstants);
	}

	return true;
}

D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS Camera::CheckMsaaQuality(int _sampleCount, DXGI_FORMAT _format)
{
	HRESULT hr = S_OK;

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS aaData;
	ZeroMemory(&aaData, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS));
	aaData.SampleCount = _sampleCount;
	aaData.Format = _format;
	LogIfFailed(GraphicManager::Instance().GetDevice()->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &aaData, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS)), hr);

	if (FAILED(hr))
	{
		return D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS();
	}

	return aaData;
}
