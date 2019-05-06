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
const static int FrameCount = 3;
#include "FrameResource.h"

// game time
#include "GameTime.h"

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
	ID3D12Device *GetDevice();
	ID3D12QueryHeap *GetGpuTimeQuery();
	ID3D12Resource *GetGpuTimeResult();
	UINT GetRtvDesciptorSize();
	UINT GetDsvDesciptorSize();
	UINT GetCbvSrvUavDesciptorSize();
	FrameResource GetFrameResource();
	GameTime GetGameTime();
	UINT64 GetGpuFreq();

private:
	HRESULT CreateGpuTimeQuery();
	HRESULT CreateGraphicCommand();
	HRESULT CreateGraphicFences();
	bool CreateGraphicThreads();

	ID3D12Device *mainDevice;
	ComPtr<ID3D12CommandQueue> mainGraphicQueue;
	ComPtr<ID3D12CommandAllocator> mainGraphicAllocator[FrameCount];
	ComPtr<ID3D12GraphicsCommandList> mainGraphicList[FrameCount];

	// for gpu time measure
	ComPtr<ID3D12QueryHeap> gpuTimeQuery;
	ComPtr<ID3D12Resource> gpuTimeResult;
	UINT64 gpuFreq;

	// suport up to 32 threads
	// however, squall graphics will only allocate threads base on number of logical cores
	// 1st handle will be used as render thread, and others are worker thread
	int numOfLogicalCores;
	HANDLE beginRenderThread;
	HANDLE renderThreadHandles;

	// frame index and fences
	int currFrameIndex;
	ComPtr<ID3D12Fence> mainGraphicFence;
	HANDLE mainFenceEvent;				// fence event handle for sync
	UINT64 graphicFences[FrameCount];	// use for frame list
	UINT64 mainFenceValue;				// use for init/destroy

										// if init succeed
	bool initSucceed;
	GameTime gameTime;

	// descriptor size
	UINT rtvDescriptorSize;
	UINT dsvDescriptorSize;
	UINT cbvSrvUavDescriptorSize;
};