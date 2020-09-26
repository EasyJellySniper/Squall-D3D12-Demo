#pragma once
#include "GameTime.h"
#include "FrameResource.h"
#include <chrono>
using namespace std;

struct GameTime
{
	double updateTime;
	double renderTime;
	double cullingTime;
	double sortingTime;
	double uploadTime;
	int batchCount[MAX_WORKER_THREAD_COUNT];
	double renderThreadTime[MAX_WORKER_THREAD_COUNT];
};

enum GpuTimeType
{
	BeginFrame = 0, PrepassWork, UpdateTopLevelAS, TileLightCulling, CollectShadowMap, RayTracingShadow, SkyboxPass, TransparentPass, EndFrame, Count
};

class GameTimerManager
{
public:
	GameTimerManager(const GameTimerManager&) = delete;
	GameTimerManager(GameTimerManager&&) = delete;
	GameTimerManager& operator=(const GameTimerManager&) = delete;
	GameTimerManager& operator=(GameTimerManager&&) = delete;

	GameTimerManager() {}
	~GameTimerManager() {}
	void Init();
	void Release();
	void CalcGpuTime();

	static GameTimerManager& Instance()
	{
		static GameTimerManager instance;
		return instance;
	}

	void PrintGameTime();

#if defined(GRAPHICTIME)
	GameTime gameTime;
	chrono::steady_clock::time_point lastTime;
	double profileTime = 0.0;
	ComPtr<ID3D12Resource> gpuTimeResult[GpuTimeType::Count];
	ComPtr<ID3D12Resource> gpuTimeOpaque[MAX_WORKER_THREAD_COUNT];
	ComPtr<ID3D12Resource> gpuTimeCutout[MAX_WORKER_THREAD_COUNT];
	ComPtr<ID3D12Resource> gpuTimeDepth[MAX_WORKER_THREAD_COUNT];
	double gpuTimeMs[GpuTimeType::Count];
	double gpuTimeOpaqueMs;
	double gpuTimeCutoutMs;
	double gpuTimeDepthMs;
	double totalGpuMs;
#endif
};