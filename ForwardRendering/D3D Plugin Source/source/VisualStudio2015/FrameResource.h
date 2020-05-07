#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
using namespace Microsoft::WRL;
using namespace DirectX;

const static int FrameCount = 3;

struct FrameResource
{
	ID3D12CommandQueue* mainGraphicQueue;
	ID3D12CommandAllocator* mainGraphicAllocator;
	ID3D12GraphicsCommandList* mainGraphicList;
	int numOfLogicalCores;
	HANDLE beginRenderThread;
	HANDLE renderThreadHandles;
};

struct SystemConstant
{
	XMFLOAT4X4 sqMatrixMvp;
};