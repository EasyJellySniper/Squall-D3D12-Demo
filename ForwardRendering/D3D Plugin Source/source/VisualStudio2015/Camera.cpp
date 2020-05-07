#include "Camera.h"
#include "stdafx.h"
#include "GraphicManager.h"
#include "d3dx12.h"
#include "MeshManager.h"
#include "ShaderManager.h"

bool Camera::Initialize(CameraData _cameraData)
{
	cameraData = _cameraData;
	numOfRenderTarget = 0;
	msaaQuality = 0;

	for (int i = 0; i < MaxRenderTargets; i++)
	{
		renderTarget.push_back((ID3D12Resource*)cameraData.renderTarget[i]);
	}

	for (int i = 0; i < MaxRenderTargets; i++)
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

	if (!CreateDebugMaterial())
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

	for (size_t i = 0; i < msaaTarget.size(); i++)
	{
		msaaTarget[i].Reset();
	}
	msaaDepthTarget.Reset();

	renderTarget.clear();
	msaaTarget.clear();

	debugWireFrame.Release();
}

CameraData Camera::GetCameraData()
{
	return cameraData;
}

ID3D12Resource * Camera::GetRtvSrc(int _index)
{
	return renderTarget[_index];
}

ID3D12Resource * Camera::GetDsvSrc()
{
	return depthTarget;
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

void Camera::SetViewProj(XMFLOAT4X4 _view, XMFLOAT4X4 _proj)
{
	// data from unity is column major, while d3d matrix use row major
	viewMatrix = _view;
	projMatrix = _proj;
}

void Camera::SetViewPortScissorRect(D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect)
{
	viewPort = _viewPort;
	scissorRect = _scissorRect;
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

Material Camera::GetDebugMaterial()
{
	return debugWireFrame;
}

HRESULT Camera::CreateRtvDescriptorHeaps()
{
	HRESULT hr = S_OK;

	// create render target handle
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = MaxRenderTargets;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHandle.GetAddressOf())), hr);

	// create aa target handle if necessary
	if (cameraData.allowMSAA > 1)
	{
		rtvHeapDesc.NumDescriptors = MaxRenderTargets;
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

	if (cameraData.allowMSAA == 1)
	{
		LogIfFailed(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHandle.GetAddressOf())), hr);
	}
	else
	{
		LogIfFailed(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(msaaDsvHandle.GetAddressOf())), hr);
	}

	return hr;
}

void Camera::CreateRtv()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvHandle->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < MaxRenderTargets; i++)
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
	depthStencilViewDesc.ViewDimension = (cameraData.allowMSAA > 1) ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;

	if (cameraData.allowMSAA == 1)
	{
		GraphicManager::Instance().GetDevice()->CreateDepthStencilView(depthTarget, &depthStencilViewDesc, dsvHandle->GetCPUDescriptorHandleForHeapStart());
	}
	else
	{
		GraphicManager::Instance().GetDevice()->CreateDepthStencilView(msaaDepthTarget.Get(), &depthStencilViewDesc, msaaDsvHandle->GetCPUDescriptorHandleForHeapStart());
	}
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

bool Camera::CreateDebugMaterial()
{
	// create debug pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	// create debug wire frame shader
	Shader *wireFrameShader = ShaderManager::Instance().CompileShader(L"DebugWireFrame.hlsl", "WireFrameVS", "WireFramePS");
	if (wireFrameShader == nullptr)
	{
		LogMessage(L"[SqGraphic Error]: Shader error.");
		return false;
	}

	desc.pRootSignature = ShaderManager::Instance().GetDefaultRS();
	desc.VS.BytecodeLength = wireFrameShader->GetVS()->GetBufferSize();
	desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(wireFrameShader->GetVS()->GetBufferPointer());
	desc.PS.BytecodeLength = wireFrameShader->GetPS()->GetBufferSize();
	desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(wireFrameShader->GetPS()->GetBufferPointer());
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.FrontCounterClockwise = true;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

	auto layout = MeshManager::Instance().GetDefaultInputLayout();
	desc.InputLayout.pInputElementDescs = layout.data();
	desc.InputLayout.NumElements = (UINT)layout.size();
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = numOfRenderTarget;

	for (int i = 0; i < numOfRenderTarget; i++)
	{
		desc.RTVFormats[i] = renderTarrgetDesc[i].Format;
	}

	desc.DSVFormat = depthTargetDesc.Format;
	desc.SampleDesc.Count = cameraData.allowMSAA;
	desc.SampleDesc.Quality = msaaQuality;

	MaterialData md;
	md.graphicPipeline = desc;

	return debugWireFrame.Initialize(md);
}
