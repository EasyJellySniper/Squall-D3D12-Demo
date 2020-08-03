#pragma once
#include <d3d12.h>
#include "stdafx.h"
#include <wrl.h>
using namespace Microsoft::WRL;

class Texture
{
public:
	void SetInstanceID(int _id);
	int GetInstanceID();

	void Release();
	void SetResource(ID3D12Resource* _data);
	ID3D12Resource* GetResource();

	void SetFormat(DXGI_FORMAT _format);
	DXGI_FORMAT GetFormat();

	void InitRTV(ID3D12Resource** _rtv, DXGI_FORMAT _format, int _numRT, bool _msaa);
	void InitDSV(ID3D12Resource** _dsv, DXGI_FORMAT _format, int _numRT, bool _msaa);
	void InitSRV(ID3D12Resource** _srv, DXGI_FORMAT _format, int _numRT, bool _msaa, bool _isCube = false);
	void InitSRV(ID3D12Resource** _srv, DXGI_FORMAT *_format, bool *_uavList, int _numRT);

	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvCPU(int _index);
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCPU(int _index);
	ID3D12DescriptorHeap* GetSrv();

	ID3D12Resource* GetRtvSrc(int _index);
	ID3D12Resource* GetDsvSrc(int _index);
	ID3D12Resource* GetSrvSrc(int _index);

protected:
	int instanceID;

	ID3D12Resource* texResource;
	DXGI_FORMAT texFormat;

	// normally managed by manager, but allow indenpendent descriptor also
	ComPtr<ID3D12DescriptorHeap> rtvHandle;
	ComPtr<ID3D12DescriptorHeap> dsvHandle;
	ComPtr<ID3D12DescriptorHeap> srvHandle;
	ComPtr<ID3D12DescriptorHeap> uavHandle;

	ID3D12Resource** rtvSrc;
	ID3D12Resource** dsvSrc;
	ID3D12Resource** srvSrc;
	ID3D12Resource** uavSrc;
};