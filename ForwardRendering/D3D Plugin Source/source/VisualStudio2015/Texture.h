#pragma once
#include <d3d12.h>
#include "stdafx.h"
#include <wrl.h>
#include <vector>
using namespace std;
using namespace Microsoft::WRL;

class Texture
{
public:
	Texture(int _numRtvHeap, int _numDsvHeap, int _numCbvSrvUavHeap);
	void SetInstanceID(int _id);
	int GetInstanceID();

	void Release();
	void SetResource(ID3D12Resource* _data);
	ID3D12Resource* GetResource();

	void SetFormat(DXGI_FORMAT _format);
	DXGI_FORMAT GetFormat();

	int InitRTV(ID3D12Resource* _rtv, DXGI_FORMAT _format, bool _msaa);
	int InitDSV(ID3D12Resource* _dsv, DXGI_FORMAT _format, bool _msaa);
	int InitSRV(ID3D12Resource* _srv, DXGI_FORMAT _format, bool _msaa, bool _isCube = false);
	int InitUAV(ID3D12Resource* _uav, DXGI_FORMAT _format);

	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvCPU(int _index);
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCPU(int _index);
	ID3D12DescriptorHeap* GetSrv();

	ID3D12Resource* GetRtvSrc(int _index);
	ID3D12Resource* GetDsvSrc(int _index);
	ID3D12Resource* GetSrvSrc(int _index);

protected:
	int instanceID;

	// use for texture manager (global heap)
	ID3D12Resource* texResource;
	DXGI_FORMAT texFormat;

	// normally managed by manager, but allow indenpendent descriptor also
	ComPtr<ID3D12DescriptorHeap> rtvHandle;
	ComPtr<ID3D12DescriptorHeap> dsvHandle;
	ComPtr<ID3D12DescriptorHeap> cbvSrvUavHandle;

	vector<ID3D12Resource*> rtvSrc;
	vector<ID3D12Resource*> dsvSrc;
	vector<ID3D12Resource*> cbvSrvUavSrc;

	int rtvCount;
	int dsvCount;
	int cbvSrvUavCount;
};