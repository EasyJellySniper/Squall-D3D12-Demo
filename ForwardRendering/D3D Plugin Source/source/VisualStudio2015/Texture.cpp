#include "Texture.h"
#include "GraphicManager.h"

void Texture::SetInstanceID(int _id)
{
	instanceID = _id;
}

int Texture::GetInstanceID()
{
	return instanceID;
}

void Texture::Release()
{
	if (rtvSrc != nullptr)
	{
		delete[]rtvSrc;
		rtvSrc = nullptr;
	}

	if (dsvSrc != nullptr)
	{
		delete[]dsvSrc;
		dsvSrc = nullptr;
	}

	if (srvSrc != nullptr)
	{
		delete[]srvSrc;
		srvSrc = nullptr;
	}

	rtvHandle.Reset();
	dsvHandle.Reset();
	srvHandle.Reset();
}

void Texture::SetResource(ID3D12Resource* _data)
{
	texResource = _data;
}

ID3D12Resource* Texture::GetResource()
{
	return texResource;
}

void Texture::SetFormat(DXGI_FORMAT _format)
{
	texFormat = _format;
}

DXGI_FORMAT Texture::GetFormat()
{
	return texFormat;
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRtvCPU(int _index)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(rtvHandle->GetCPUDescriptorHandleForHeapStart());
	return handle.Offset(_index, GraphicManager::Instance().GetRtvDesciptorSize());
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetDsvCPU(int _index)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(dsvHandle->GetCPUDescriptorHandleForHeapStart());
	return handle.Offset(_index, GraphicManager::Instance().GetDsvDesciptorSize());
}

ID3D12DescriptorHeap* Texture::GetSrv()
{
	return srvHandle.Get();
}

ID3D12Resource* Texture::GetRtvSrc(int _index)
{
	return rtvSrc[_index];
}

ID3D12Resource* Texture::GetDsvSrc(int _index)
{
	return dsvSrc[_index];
}

ID3D12Resource* Texture::GetSrvSrc(int _index)
{
	return srvSrc[_index];
}

void Texture::InitRTV(ID3D12Resource** _rtv, DXGI_FORMAT _format, int _numRT, bool _msaa)
{
	rtvSrc = new ID3D12Resource * [_numRT];
	for (int i = 0; i < _numRT; i++)
	{
		rtvSrc[i] = _rtv[i];
	}

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

		GraphicManager::Instance().GetDevice()->CreateRenderTargetView(rtvSrc[i], &rtvDesc, rHandle);
		rHandle.Offset(1, GraphicManager::Instance().GetRtvDesciptorSize());
	}
}

void Texture::InitDSV(ID3D12Resource** _dsv, DXGI_FORMAT _format, int _numRT, bool _msaa)
{
	dsvSrc = new ID3D12Resource * [_numRT];
	for (int i = 0; i < _numRT; i++)
	{
		dsvSrc[i] = _dsv[i];
	}

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

		GraphicManager::Instance().GetDevice()->CreateDepthStencilView(dsvSrc[i], &depthStencilViewDesc, dHandle);
		dHandle.Offset(1, GraphicManager::Instance().GetDsvDesciptorSize());
	}
}

void Texture::InitSRV(ID3D12Resource** _srv, DXGI_FORMAT _format, int _numRT, bool _msaa)
{
	srvSrc = new ID3D12Resource * [_numRT];
	for (int i = 0; i < _numRT; i++)
	{
		srvSrc[i] = _srv[i];
	}

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

		GraphicManager::Instance().GetDevice()->CreateShaderResourceView(srvSrc[i], &srvDesc, sHandle);
		sHandle.Offset(1, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	}
}