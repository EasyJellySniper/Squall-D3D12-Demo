#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;
#include "stdafx.h"

class RenderTexture
{
public:
	void InitRTV(ID3D12Resource* _rtv, DXGI_FORMAT _format, bool _msaa = false);
	void InitDSV(ID3D12Resource* _dsv, DXGI_FORMAT _format, bool _msaa = false);
	void InitSRV(ID3D12Resource* _srv, DXGI_FORMAT _format, bool _msaa = false);
	void Release();

	D3D12_GPU_DESCRIPTOR_HANDLE GetRtvGPU();
	D3D12_GPU_DESCRIPTOR_HANDLE GetDsvGPU();
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGPU();

private:
	ComPtr<ID3D12DescriptorHeap> rtvHandle;
	ComPtr<ID3D12DescriptorHeap> dsvHandle;
	ComPtr<ID3D12DescriptorHeap> srvHandle;

	ID3D12Resource* rtvSrc;
	ID3D12Resource* dsvSrc;
	ID3D12Resource* srvSrc;
};