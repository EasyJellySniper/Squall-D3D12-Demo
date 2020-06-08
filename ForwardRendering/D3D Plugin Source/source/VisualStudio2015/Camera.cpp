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
		renderTarrgetDesc[i].Format = GetColorFormat(srcDesc.Format);

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
		aaData = CheckMsaaQuality(cameraData.allowMSAA, GetDepthFormat(srcDesc.Format));
		srcDesc.SampleDesc.Quality = max(aaData.NumQualityLevels - 1, 0);

		optClearDepth.DepthStencil.Depth = 0.0f;
		optClearDepth.DepthStencil.Stencil = 0;
		optClearDepth.Format = GetDepthFormat(srcDesc.Format);

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

	// render target is valid, we can create descriptors now
	if (FAILED(CreateRtvDescriptorHeaps()))
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Create Rtv Descriptor failed.");
		return false;
	}

	if (FAILED(CreateDsvDescriptorHeaps()))
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Create Dsv Descriptor failed.");
		return false;
	}

	CreateRtv();
	CreateDsv();

	if (!CreatePipelineMaterial())
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Create Debug Material failed.");
		return false;
	}

	return true;
}

void Camera::Release()
{
	rtvHandle.Reset();
	msaaRtvHandle.Reset();
	dsvHandle.Reset();
	msaaDsvHandle.Reset();
	depthSrvHandle.Reset();
	msDepthSrvHandle.Reset();

	for (size_t i = 0; i < msaaTarget.size(); i++)
	{
		msaaTarget[i].Reset();
	}
	msaaDepthTarget.Reset();

	renderTarget.clear();
	msaaTarget.clear();

	for (auto& m : pipelineMaterials)
	{
		for (int i = 0; i < MAX_CULL_MODE; i++)
		{
			m.second[i].Release();
		}
		m.second.clear();
	}
	pipelineMaterials.clear();
	resolveDepthMaterial.Release();
}

CameraData Camera::GetCameraData()
{
	return cameraData;
}

ID3D12Resource * Camera::GetRtvSrc(int _index)
{
	return renderTarget[_index];
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

ID3D12DescriptorHeap * Camera::GetRtv()
{
	return rtvHandle.Get();
}

ID3D12DescriptorHeap * Camera::GetMsaaRtv()
{
	return msaaRtvHandle.Get();
}

ID3D12DescriptorHeap * Camera::GetDsv()
{
	return dsvHandle.Get();
}

ID3D12DescriptorHeap * Camera::GetMsaaDsv()
{
	return msaaDsvHandle.Get();
}

ID3D12DescriptorHeap* Camera::GetMsaaSrv()
{
	return msDepthSrvHandle.Get();
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
	return depthTarget;
}

bool Camera::FrustumTest(BoundingBox _bound)
{
	return (camFrustum.Contains(_bound) != DirectX::DISJOINT);
}

HRESULT Camera::CreateRtvDescriptorHeaps()
{
	HRESULT hr = S_OK;

	// create render target handle
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = MAX_RENDER_TARGETS;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHandle.GetAddressOf())), hr);

	// create aa target handle if necessary
	if (cameraData.allowMSAA > 1)
	{
		rtvHeapDesc.NumDescriptors = MAX_RENDER_TARGETS;
		LogIfFailed(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(msaaRtvHandle.GetAddressOf())), hr);
	}

	return hr;
}

HRESULT Camera::CreateDsvDescriptorHeaps()
{
	HRESULT hr = S_OK;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;

	// Common Depth
	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHandle.GetAddressOf())), hr);
	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(depthSrvHandle.GetAddressOf())), hr);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;	// assume 32 bit depth
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	GraphicManager::Instance().GetDevice()->CreateShaderResourceView(depthTarget, &srvDesc, depthSrvHandle->GetCPUDescriptorHandleForHeapStart());

	// MSAA Depth
	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(msaaDsvHandle.GetAddressOf())), hr);
	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(msDepthSrvHandle.GetAddressOf())), hr);

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;	// assume 32 bit depth
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;

	GraphicManager::Instance().GetDevice()->CreateShaderResourceView(msaaDepthTarget.Get(), &srvDesc, msDepthSrvHandle->GetCPUDescriptorHandleForHeapStart());

	return hr;
}

void Camera::CreateRtv()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvHandle->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < MAX_RENDER_TARGETS; i++)
	{
		if (renderTarget[i] != nullptr)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.Format = renderTarrgetDesc[i].Format;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = 0;
			rtvDesc.Texture2D.PlaneSlice = 0;

			GraphicManager::Instance().GetDevice()->CreateRenderTargetView(renderTarget[i], &rtvDesc, rHandle);
		}
		rHandle.Offset(1, GraphicManager::Instance().GetRtvDesciptorSize());
	}

	if (cameraData.allowMSAA > 1)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE aaHandle(msaaRtvHandle->GetCPUDescriptorHandleForHeapStart());
		for (size_t i = 0; i < msaaTarget.size(); i++)
		{
			if (msaaTarget[i] != nullptr)
			{
				D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
				rtvDesc.Format = renderTarrgetDesc[i].Format;
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

				GraphicManager::Instance().GetDevice()->CreateRenderTargetView(msaaTarget[i].Get(), &rtvDesc, aaHandle);
			}
			aaHandle.Offset(1, GraphicManager::Instance().GetRtvDesciptorSize());
		}
	}
}

void Camera::CreateDsv()
{
	D3D12_RESOURCE_DESC dsvRrcDesc = depthTarget->GetDesc();
	depthTargetDesc = dsvRrcDesc;
	depthTargetDesc.Format = GetDepthFormat(dsvRrcDesc.Format);
	DXGI_FORMAT depthStencilFormat = depthTargetDesc.Format;

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
	depthStencilViewDesc.Format = depthStencilFormat;
	depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	GraphicManager::Instance().GetDevice()->CreateDepthStencilView(depthTarget, &depthStencilViewDesc, dsvHandle->GetCPUDescriptorHandleForHeapStart());

	depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
	GraphicManager::Instance().GetDevice()->CreateDepthStencilView(msaaDepthTarget.Get(), &depthStencilViewDesc, msaaDsvHandle->GetCPUDescriptorHandleForHeapStart());
}

bool Camera::CreatePipelineMaterial()
{
	// init vector
	for (int i = 0; i < MaterialType::EndSystemMaterial; i++)
	{
		pipelineMaterials[i].resize(MAX_CULL_MODE);
	}

	// create debug wire frame material
	Shader* wireFrameShader = ShaderManager::Instance().CompileShader(L"DebugWireFrame.hlsl");
	if (wireFrameShader != nullptr)
	{
		pipelineMaterials[MaterialType::DebugWireFrame][CullMode::Off] = MaterialManager::Instance().CreateMaterialFromShader(wireFrameShader, *this, D3D12_FILL_MODE_WIREFRAME, D3D12_CULL_MODE_NONE);
	}

	// create depth prepass material
	D3D_SHADER_MACRO depthMacro[] = { "_CUTOFF_ON","1",NULL,NULL };
	Shader* depthPrePassOpaque = ShaderManager::Instance().CompileShader(L"DepthPrePass.hlsl");
	Shader* depthPrePassCutoff = ShaderManager::Instance().CompileShader(L"DepthPrePass.hlsl", depthMacro);
	if (depthPrePassOpaque != nullptr)
	{
		for (int i = 0; i < MAX_CULL_MODE; i++)
		{
			pipelineMaterials[MaterialType::DepthPrePassOpaque][i] = MaterialManager::Instance().CreateMaterialFromShader(depthPrePassOpaque, *this, D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(i + 1));
		}
	}

	if (depthPrePassCutoff != nullptr)
	{
		for (int i = 0; i < MAX_CULL_MODE; i++)
		{
			pipelineMaterials[MaterialType::DepthPrePassCutoff][i] = MaterialManager::Instance().CreateMaterialFromShader(depthPrePassCutoff, *this, D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(i + 1));
		}
	}

	Shader* resolveDepth = ShaderManager::Instance().CompileShader(L"ResolveDepth.hlsl", nullptr, true);
	if (resolveDepth != nullptr)
	{
		resolveDepthMaterial = MaterialManager::Instance().CreateMaterialPost(resolveDepth, *this, true);

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

DXGI_FORMAT Camera::GetColorFormat(DXGI_FORMAT _typelessFormat)
{
	DXGI_FORMAT colorFormat = DXGI_FORMAT_UNKNOWN;
	if (_typelessFormat == DXGI_FORMAT_R16G16B16A16_TYPELESS)
	{
		colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	}

	return colorFormat;
}

DXGI_FORMAT Camera::GetDepthFormat(DXGI_FORMAT _typelessFormat)
{
	DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_UNKNOWN;

	// need to choose depth buffer according to input
	if (_typelessFormat == DXGI_FORMAT_R32G8X24_TYPELESS)
	{
		// 32 bit depth buffer
		depthStencilFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	}
	else if (_typelessFormat == DXGI_FORMAT_R24G8_TYPELESS)
	{
		// 24 bit depth buffer
		depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	}
	else if (_typelessFormat == DXGI_FORMAT_R16_TYPELESS)
	{
		// 16 bit depth buffer
		depthStencilFormat = DXGI_FORMAT_D16_UNORM;
	}
	else
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Unknown Depth Target Format " + to_wstring(_typelessFormat) + L".");
	}

	return depthStencilFormat;
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
