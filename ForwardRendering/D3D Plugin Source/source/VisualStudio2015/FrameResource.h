#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
using namespace Microsoft::WRL;
using namespace DirectX;

const static int MAX_FRAME_COUNT = 3;
const static int MAX_WORKER_THREAD_COUNT = 32;

struct FrameResource
{
	ID3D12CommandQueue* mainGraphicQueue;
	ID3D12CommandAllocator* mainGraphicAllocator;
	ID3D12GraphicsCommandList* mainGraphicList;
};

struct SystemConstant
{
	XMFLOAT4X4 sqMatrixMvp;
};