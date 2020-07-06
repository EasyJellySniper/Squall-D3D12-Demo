#pragma once
#include <assert.h>
#include <d3d12.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <process.h>
using namespace std;

#include <wrl.h>
using namespace Microsoft::WRL;

// frame resource
#include "FrameResource.h"
#include "UploadBuffer.h"

// game time
#include "GameTimerManager.h"
#include "CameraManager.h"

// setup thread parameter
struct ThreadParameter
{
	int threadIndex;
};

class GraphicManager
{
public:
	GraphicManager(const GraphicManager&) = delete;
	GraphicManager(GraphicManager&&) = delete;
	GraphicManager& operator=(const GraphicManager&) = delete;
	GraphicManager& operator=(GraphicManager&&) = delete;

	static GraphicManager& Instance()
	{
		static GraphicManager instance;
		return instance;
	}

	GraphicManager() {}
	~GraphicManager() {}

	bool Initialize(ID3D12Device* _device, int _numOfThreads);
	void Release();
	int GetThreadCount();
	void Update();
	void Render();
	void RenderThread();
	void WaitForRenderThread();
	void WaitForGPU();
	void ExecuteCommandList(int _listCount, ID3D12CommandList **_cmdList);
	ID3D12Device *GetDevice();
	ID3D12QueryHeap *GetGpuTimeQuery();
	ID3D12Resource *GetGpuTimeResult();
	UINT GetRtvDesciptorSize();
	UINT GetDsvDesciptorSize();
	UINT GetCbvSrvUavDesciptorSize();
	FrameResource GetFrameResource();
	GameTime GetGameTime();
	UINT64 GetGpuFreq();
	void WaitBeginWorkerThread(int _index);
	void SetBeginWorkerThreadEvent();
	void ResetWorkerThreadFinish();
	void WaitForWorkerThread();
	void SetWorkerThreadFinishEvent(int _index);
	void UploadSystemConstant(SystemConstant _sc, int _frameIdx);
	D3D12_GPU_VIRTUAL_ADDRESS GetSystemConstant(int _frameIdx);

private:
	HRESULT CreateGpuTimeQuery();
	HRESULT CreateGraphicCommand();
	HRESULT CreateGfxAllocAndList(ComPtr<ID3D12CommandAllocator>& _allocator, ComPtr<ID3D12GraphicsCommandList>& _list);
	HRESULT CreateGraphicFences();
	bool CreateGraphicThreads();
	void DrawCamera();

	ID3D12Device *mainDevice;
	ComPtr<ID3D12CommandQueue> mainGraphicQueue;
	ComPtr<ID3D12CommandAllocator> preGfxAllocator[MAX_FRAME_COUNT];
	ComPtr<ID3D12GraphicsCommandList> preGfxList[MAX_FRAME_COUNT];
	ComPtr<ID3D12CommandAllocator> midGfxAllocator[MAX_FRAME_COUNT];
	ComPtr<ID3D12GraphicsCommandList> midGfxList[MAX_FRAME_COUNT];
	ComPtr<ID3D12CommandAllocator> postGfxAllocator[MAX_FRAME_COUNT];
	ComPtr<ID3D12GraphicsCommandList> postGfxList[MAX_FRAME_COUNT];
	ComPtr<ID3D12CommandAllocator> workerGfxAllocator[MAX_WORKER_THREAD_COUNT][MAX_FRAME_COUNT];	// allow multiple-thread gfx
	ComPtr<ID3D12GraphicsCommandList> workerGfxList[MAX_WORKER_THREAD_COUNT][MAX_FRAME_COUNT];

	// for gpu time measure
	ComPtr<ID3D12QueryHeap> gpuTimeQuery;
	ComPtr<ID3D12Resource> gpuTimeResult;
	UINT64 gpuFreq;

	// render thread
	int numOfLogicalCores;
	HANDLE beginRenderThread;
	HANDLE renderThreadHandle;
	HANDLE renderThreadFinish;
	HANDLE beginWorkerThread[MAX_WORKER_THREAD_COUNT];
	HANDLE workerThreadHandle[MAX_WORKER_THREAD_COUNT];
	HANDLE workerThreadFinish[MAX_WORKER_THREAD_COUNT];
	ThreadParameter threadParams[MAX_WORKER_THREAD_COUNT];

	// frame index and fences
	int currFrameIndex;
	ComPtr<ID3D12Fence> mainGraphicFence;
	HANDLE mainFenceEvent;				// fence event handle for sync
	UINT64 graphicFences[MAX_FRAME_COUNT];	// use for frame list
	UINT64 mainFenceValue;				// use for init/destroy

										// if init succeed
	bool initSucceed;

	// descriptor size
	UINT rtvDescriptorSize;
	UINT dsvDescriptorSize;
	UINT cbvSrvUavDescriptorSize;

	// camera cache
	Camera activeCam;
	SystemConstant systemConstantCPU;

	// system constant
	unique_ptr<UploadBuffer<SystemConstant>> systemConstantGPU[MAX_FRAME_COUNT];
};