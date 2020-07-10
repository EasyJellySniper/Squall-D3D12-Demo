#include "RenderTexture.h"
#include "GraphicManager.h"

void RenderTexture::InitRTV(ID3D12Resource* _rtv, DXGI_FORMAT _format, bool _msaa, int _numRT)
{
	rtvSrc = _rtv;

	// create RTV handle
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = _numRT;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHandle.GetAddressOf())));

	// create rtv
	CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvHandle->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < _numRT; i++)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = _format;
		rtvDesc.ViewDimension = (_msaa) ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		GraphicManager::Instance().GetDevice()->CreateRenderTargetView(rtvSrc, &rtvDesc, rHandle);
		rHandle.Offset(1, GraphicManager::Instance().GetRtvDesciptorSize());
	}
}

void RenderTexture::InitDSV(ID3D12Resource* _dsv, DXGI_FORMAT _format, bool _msaa, int _numRT)
{
	dsvSrc = _dsv;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = _numRT;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHandle.GetAddressOf())));

	// create dsv
	CD3DX12_CPU_DESCRIPTOR_HANDLE dHandle(dsvHandle->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < _numRT; i++)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Format = _format;
		depthStencilViewDesc.ViewDimension = (_msaa) ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;

		GraphicManager::Instance().GetDevice()->CreateDepthStencilView(dsvSrc, &depthStencilViewDesc, dHandle);
		dHandle.Offset(1, GraphicManager::Instance().GetDsvDesciptorSize());
	}
}

void RenderTexture::InitSRV(ID3D12Resource* _srv, DXGI_FORMAT _format, bool _msaa, int _numRT)
{
	srvSrc = _srv;

	// create SRV handle
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
	srvHeapDesc.NumDescriptors = _numRT;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(srvHandle.GetAddressOf())));

	// create srv
	CD3DX12_CPU_DESCRIPTOR_HANDLE sHandle(srvHandle->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < _numRT; i++)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = _format;
		srvDesc.ViewDimension = (_msaa) ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = -1;

		GraphicManager::Instance().GetDevice()->CreateShaderResourceView(srvSrc, &srvDesc, sHandle);
		sHandle.Offset(1, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	}
}

void RenderTexture::Release()
{
	rtvHandle.Reset();
	dsvHandle.Reset();
	srvHandle.Reset();
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTexture::GetRtvCPU()
{
	return rtvHandle->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTexture::GetDsvCPU()
{
	return dsvHandle->GetCPUDescriptorHandleForHeapStart();
}

ID3D12DescriptorHeap* RenderTexture::GetSrv()
{
	return srvHandle.Get();
}

ID3D12Resource* RenderTexture::GetRtvSrc()
{
	return rtvSrc;
}

ID3D12Resource* RenderTexture::GetDsvSrc()
{
	return dsvSrc;
}

ID3D12Resource* RenderTexture::GetSrvSrc()
{
	return srvSrc;
}
