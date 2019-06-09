#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

struct FrameResource
{
	ID3D12CommandQueue* mainGraphicQueue;
	ID3D12CommandAllocator* mainGraphicAllocator;
	ID3D12GraphicsCommandList* mainGraphicList;
	int numOfLogicalCores;
	HANDLE beginRenderThread;
	HANDLE renderThreadHandles;
};