#pragma once
#include <d3d12.h>
#include "d3dx12.h"
#include "stdafx.h"
#include <wrl.h>
using namespace Microsoft::WRL;

class DefaultBuffer
{
public:

	// entry for buffer
	DefaultBuffer(ID3D12Device *_device, UINT64 _bufferSize, bool _isUAV, D3D12_RESOURCE_STATES _states = D3D12_RESOURCE_STATE_COMMON)
	{
		auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		if (_isUAV)
		{
			flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(_bufferSize, flags);

		LogIfFailedWithoutHR(_device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			_states,
			nullptr,
			IID_PPV_ARGS(defaultBuffer.GetAddressOf())));
	}

	// entry for texture
	DefaultBuffer(ID3D12Device* _device, bool _isUAV, DXGI_FORMAT _format, UINT64 _width, UINT _height, D3D12_RESOURCE_STATES _states = D3D12_RESOURCE_STATE_COMMON
		, D3D12_CLEAR_VALUE *_clearValue = nullptr)
	{
		auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		if (_isUAV)
		{
			flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(_format, _width, _height, 1, (_isUAV) ? 1 : 0, 1, 0, flags);

		LogIfFailedWithoutHR(_device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			_states,
			_clearValue,
			IID_PPV_ARGS(defaultBuffer.GetAddressOf())));
	}

	~DefaultBuffer()
	{
		defaultBuffer.Reset();
	}

	DefaultBuffer(const DefaultBuffer& rhs) = delete;
	DefaultBuffer& operator=(const DefaultBuffer& rhs) = delete;

	ID3D12Resource* Resource()
	{
		return defaultBuffer.Get();
	}

private:
	ComPtr<ID3D12Resource> defaultBuffer;
};