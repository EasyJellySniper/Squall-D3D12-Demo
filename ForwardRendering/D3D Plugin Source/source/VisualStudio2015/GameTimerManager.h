#pragma once
#include "GameTime.h"

struct GameTime
{
	double updateTime;
	double renderTime;
	double renderThreadTime;
	double gpuTime;
	double cullingTime;
	int batchCount;
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

	GameTime gameTime;
};