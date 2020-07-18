#include "GameTimerManager.h"
#include <iostream>
#include <iomanip>
using namespace std;
#include "GraphicManager.h"

void GameTimerManager::PrintGameTime()
{
    chrono::steady_clock::time_point currTime = std::chrono::steady_clock::now();
    profileTime += std::chrono::duration_cast<std::chrono::milliseconds>(currTime - lastTime).count();

	if (profileTime > 1000.0)
	{
		cout << "-------------- CPU Profile(ms) --------------\n";
		cout << fixed << setprecision(4);
		cout << "Wait GPU Fence: " << gameTime.updateTime << endl;
		cout << "Culling Time: " << gameTime.cullingTime << endl;
		cout << "Sorting Time: " << gameTime.sortingTime << endl;
		cout << "Render Time: " << gameTime.renderTime << endl;

		int totalDrawCall = 0;
		for (int i = 0; i < GraphicManager::Instance().GetThreadCount() - 1; i++)
		{
			cout << "\tThread " << i << " Time:" << gameTime.renderThreadTime[i] << endl;
			totalDrawCall += gameTime.batchCount[i];
		}

		cout << "Total DrawCall: " << totalDrawCall << endl;

		cout << "-------------- GPU Profile(ms) --------------\n";
		cout << "GPU Time: " << gameTime.gpuTime << endl;

		cout << endl << endl;
        profileTime = 0.0;
	}

    lastTime = std::chrono::steady_clock::now();
}
