#pragma once
#include "GameTime.h"
#include "FrameResource.h"
#include <chrono>
using namespace std;

struct GameTime
{
	double updateTime;
	double renderTime;
	double gpuTime;
	double cullingTime;
	double sortingTime;
	int batchCount[MAX_WORKER_THREAD_COUNT];
	double renderThreadTime[MAX_WORKER_THREAD_COUNT];
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
#endif
};