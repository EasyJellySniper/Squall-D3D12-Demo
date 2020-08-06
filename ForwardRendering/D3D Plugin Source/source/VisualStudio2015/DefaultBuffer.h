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
	DefaultBuffer(ID3D12Device *_device, UINT64 _bufferSize, D3D12_RESOURCE_STATES _states = D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAGS _flags = D3D12_RESOURCE_FLAG_NONE)
	{
		auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(_bufferSize, _flags);

		LogIfFailedWithoutHR(_device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			_states,
			nullptr,
			IID_PPV_ARGS(defaultBuffer.GetAddressOf())));
	}

	// entry for texture
	DefaultBuffer(ID3D12Device* _device, D3D12_RESOURCE_DESC _desc, D3D12_RESOURCE_STATES _states = D3D12_RESOURCE_STATE_COMMON, D3D12_CLEAR_VALUE *_clearValue = nullptr)
	{
		auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		LogIfFailedWithoutHR(_device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&_desc,
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