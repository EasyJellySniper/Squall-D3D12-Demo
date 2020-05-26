#pragma once
#include "GameTime.h"
#include "FrameResource.h"

struct GameTime
{
	double updateTime;
	double renderTime;
	double gpuTime;
	double cullingTime;
	int batchCount;
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

#if defined(GRAPHICTIME)
	GameTime gameTime;
#endif
};