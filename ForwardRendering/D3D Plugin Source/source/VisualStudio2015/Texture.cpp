#include "Texture.h"
#include "GraphicManager.h"

Texture::Texture(int _numRtvHeap, int _numDsvHeap, int _numCbvSrvUavHeap)
{
	rtvCount = 0;
	dsvCount = 0;
	cbvSrvUavCount = 0;
	
	// rtv handle
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = _numRtvHeap;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;

	// dsv handle
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = _numDsvHeap;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	// cbv srv uav handle
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
	srvHeapDesc.NumDescriptors = _numCbvSrvUavHeap;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;

	if (_numRtvHeap > 0)
	{
		LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHandle.GetAddressOf())));
	}

	if (_numDsvHeap > 0)
	{
		LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHandle.GetAddressOf())));
	}

	if (_numCbvSrvUavHeap > 0)
	{
		LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(cbvSrvUavHandle.GetAddressOf())));
	}
}

void Texture::SetInstanceID(size_t _id)
{
	instanceID = _id;
}

size_t Texture::GetInstanceID()
{
	return instanceID;
}

void Texture::Release()
{
	rtvSrc.clear();
	dsvSrc.clear();
	cbvSrvUavSrc.clear();
	rtvHandle.Reset();
	dsvHandle.Reset();
	cbvSrvUavHandle.Reset();
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

void Texture::SetInfo(TextureInfo _info)
{
	texInfo = _info;
}

DXGI_FORMAT Texture::GetFormat()
{
	return texFormat;
}

TextureInfo Texture::GetInfo()
{
	return texInfo;
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
	return cbvSrvUavHandle.Get();
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
	return cbvSrvUavSrc[_index];
}

int Texture::InitRTV(ID3D12Resource* _rtv, DXGI_FORMAT _format, bool _msaa)
{
	rtvSrc.push_back(_rtv);

	// create rtv
	CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvHandle->GetCPUDescriptorHandleForHeapStart(), rtvCount, GraphicManager::Instance().GetRtvDesciptorSize());

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = _format;
	rtvDesc.ViewDimension = (_msaa) ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	GraphicManager::Instance().GetDevice()->CreateRenderTargetView(rtvSrc[rtvCount], &rtvDesc, rHandle);
	return rtvCount++;
}

int Texture::InitDSV(ID3D12Resource* _dsv, DXGI_FORMAT _format, bool _msaa)
{
	dsvSrc.push_back(_dsv);

	// create dsv
	CD3DX12_CPU_DESCRIPTOR_HANDLE dHandle(dsvHandle->GetCPUDescriptorHandleForHeapStart(), dsvCount, GraphicManager::Instance().GetDsvDesciptorSize());

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
	depthStencilViewDesc.Format = _format;
	depthStencilViewDesc.ViewDimension = (_msaa) ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;

	GraphicManager::Instance().GetDevice()->CreateDepthStencilView(dsvSrc[dsvCount], &depthStencilViewDesc, dHandle);
	return dsvCount++;
}

int Texture::InitSRV(ID3D12Resource* _srv, DXGI_FORMAT _format, bool _msaa, bool _isCube)
{
	cbvSrvUavSrc.push_back(_srv);

	// create srv
	CD3DX12_CPU_DESCRIPTOR_HANDLE sHandle(cbvSrvUavHandle->GetCPUDescriptorHandleForHeapStart(), cbvSrvUavCount, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = _format;
	srvDesc.ViewDimension = (_msaa) ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.ViewDimension = (_isCube) ? D3D12_SRV_DIMENSION_TEXTURECUBE : srvDesc.ViewDimension;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	if (_isCube)
	{
		srvDesc.TextureCube.MipLevels = -1;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0;
	}

	GraphicManager::Instance().GetDevice()->CreateShaderResourceView(cbvSrvUavSrc[cbvSrvUavCount], &srvDesc, sHandle);
	return cbvSrvUavCount++;
}

int Texture::InitUAV(ID3D12Resource* _uav, DXGI_FORMAT _format)
{
	cbvSrvUavSrc.push_back(_uav);

	// create srv/uav
	CD3DX12_CPU_DESCRIPTOR_HANDLE uHandle(cbvSrvUavHandle->GetCPUDescriptorHandleForHeapStart(), cbvSrvUavCount, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	GraphicManager::Instance().GetDevice()->CreateUnorderedAccessView(cbvSrvUavSrc[cbvSrvUavCount], nullptr, &uavDesc, uHandle);
	return cbvSrvUavCount++;
}
