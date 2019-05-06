#include "Camera.h"
#include "stdafx.h"
#include "GraphicManager.h"
#include "d3dx12.h"

bool Camera::Initialize(CameraData _cameraData)
{
	cameraData = _cameraData;

	renderTarget[0] = (ID3D12Resource*)cameraData.renderTarget0;
	renderTarget[1] = (ID3D12Resource*)cameraData.renderTarget1;
	renderTarget[2] = (ID3D12Resource*)cameraData.renderTarget2;
	renderTarget[3] = (ID3D12Resource*)cameraData.renderTarget3;
	renderTarget[4] = (ID3D12Resource*)cameraData.renderTarget4;
	renderTarget[5] = (ID3D12Resource*)cameraData.renderTarget5;
	renderTarget[6] = (ID3D12Resource*)cameraData.renderTarget6;
	renderTarget[7] = (ID3D12Resource*)cameraData.renderTarget7;

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
	}

	if (cameraData.depthTarget == nullptr)
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Depth Target pointer is null.");
		return false;
	}
	depthTarget = (ID3D12Resource*)cameraData.depthTarget;

	// render target is valid, we can create descriptors now
	if (FAILED(CreateRtvDescriptorHeaps()))
	{
		return false;
	}

	if (FAILED(CreateDsvDescriptorHeaps()))
	{
		return false;
	}

	CreateRtv();
	CreateDsv();

	return true;
}

void Camera::Release()
{
	if (rtvHandle)
	{
		rtvHandle.Reset();
	}

	if (dsvHandle)
	{
		dsvHandle.Reset();
	}
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

ID3D12DescriptorHeap * Camera::GetRtv()
{
	return rtvHandle.Get();
}

ID3D12DescriptorHeap * Camera::GetDsv()
{
	return dsvHandle.Get();
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

	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHandle.GetAddressOf())), hr);

	return hr;
}

void Camera::CreateRtv()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvHandle->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < MaxRenderTargets; i++)
	{
		if (renderTarget[i] != nullptr)
		{
			D3D12_RESOURCE_DESC rtvRrcDesc = renderTarget[i]->GetDesc();

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.Format = (cameraData.allowHDR == 1) ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
			rtvDesc.ViewDimension = (cameraData.allowMSAA > 1) ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = 0;
			rtvDesc.Texture2D.PlaneSlice = 0;

			GraphicManager::Instance().GetDevice()->CreateRenderTargetView(renderTarget[i], &rtvDesc, rHandle);
		}
		rHandle.Offset(1, GraphicManager::Instance().GetRtvDesciptorSize());
	}
}

void Camera::CreateDsv()
{
	D3D12_RESOURCE_DESC dsvRrcDesc = depthTarget->GetDesc();

	DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_UNKNOWN;

	// need to choose depth buffer according to input
	if (dsvRrcDesc.Format == DXGI_FORMAT_R32G8X24_TYPELESS)
	{
		// 32 bit depth buffer
		depthStencilFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	}
	else if (dsvRrcDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
	{
		// 24 bit depth buffer
		depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	}
	else if (dsvRrcDesc.Format == DXGI_FORMAT_R16_TYPELESS)
	{
		// 16 bit depth buffer
		depthStencilFormat = DXGI_FORMAT_D16_UNORM;
	}
	else
	{
		LogMessage(L"[SqGraphic Error] SqCamera: Unknown Depth Target Format.");
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
	depthStencilViewDesc.Format = depthStencilFormat;
	depthStencilViewDesc.ViewDimension = (cameraData.allowMSAA > 1) ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;

	GraphicManager::Instance().GetDevice()->CreateDepthStencilView(depthTarget, &depthStencilViewDesc, dsvHandle->GetCPUDescriptorHandleForHeapStart());
}
