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
	ID3D12CommandAllocator* preGfxAllocator;
	ID3D12GraphicsCommandList* preGfxList;
	ID3D12CommandAllocator* midGfxAllocator;
	ID3D12GraphicsCommandList* midGfxList;
	ID3D12CommandAllocator* postGfxAllocator;
	ID3D12GraphicsCommandList* postGfxList;
	ID3D12CommandAllocator *workerGfxAlloc[MAX_WORKER_THREAD_COUNT];
	ID3D12GraphicsCommandList *workerGfxList[MAX_WORKER_THREAD_COUNT];
};

struct ObjectConstant
{
	XMFLOAT4X4 sqMatrixMvp;
	XMFLOAT4X4 sqMatrixWorld;
};

struct SystemConstant
{
	XMFLOAT3 cameraPos;
	int numDirLight;
	int numPointLight;
	int numSpotLight;
};

struct LightConstant
{
	XMFLOAT4X4 sqMatrixShadow;
};