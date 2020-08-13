#include "GameTimerManager.h"
#include <iostream>
#include <iomanip>
using namespace std;
#include "GraphicManager.h"

void GameTimerManager::Init()
{
	auto mainDevice = GraphicManager::Instance().GetDevice();

	for (int i = 0; i < GpuTimeType::Count; i++)
	{
		LogIfFailedWithoutHR(mainDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK)
			, D3D12_HEAP_FLAG_NONE
			, &CD3DX12_RESOURCE_DESC::Buffer(2 * sizeof(uint64_t))
			, D3D12_RESOURCE_STATE_COPY_DEST
			, nullptr
			, IID_PPV_ARGS(&gpuTimeResult[i])));
	}

	for (int i = 0; i < GraphicManager::Instance().GetThreadCount(); i++)
	{
		LogIfFailedWithoutHR(mainDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK)
			, D3D12_HEAP_FLAG_NONE
			, &CD3DX12_RESOURCE_DESC::Buffer(2 * sizeof(uint64_t))
			, D3D12_RESOURCE_STATE_COPY_DEST
			, nullptr
			, IID_PPV_ARGS(&gpuTimeOpaque[i])));

		LogIfFailedWithoutHR(mainDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK)
			, D3D12_HEAP_FLAG_NONE
			, &CD3DX12_RESOURCE_DESC::Buffer(2 * sizeof(uint64_t))
			, D3D12_RESOURCE_STATE_COPY_DEST
			, nullptr
			, IID_PPV_ARGS(&gpuTimeCutout[i])));
	}
}

void GameTimerManager::Release()
{
	for (int i = 0; i < GpuTimeType::Count; i++)
	{
		gpuTimeResult[i].Reset();
	}

	for (int i = 0; i < GraphicManager::Instance().GetThreadCount(); i++)
	{
		gpuTimeOpaque[i].Reset();
		gpuTimeCutout[i].Reset();
	}
}

void GameTimerManager::CalcGpuTime()
{
	totalGpuMs = 0;
	for (int i = 0; i < GpuTimeType::Count; i++)
	{
		uint64_t* pRes;
		LogIfFailedWithoutHR(gpuTimeResult[i]->Map(0, nullptr, reinterpret_cast<void**>(&pRes)));
		uint64_t t1 = pRes[0];
		uint64_t t2 = pRes[1];
		gpuTimeResult[i]->Unmap(0, nullptr);

		gpuTimeMs[i] = static_cast<double>(t2 - t1) / static_cast<double>(GraphicManager::Instance().GetGpuFreq()) * 1000.0;
		totalGpuMs += gpuTimeMs[i];
	}

	gpuTimeOpaqueMs = 0;
	gpuTimeCutoutMs = 0;
	for (int i = 0; i < GraphicManager::Instance().GetThreadCount(); i++)
	{
		// opaque time
		uint64_t* pRes;
		LogIfFailedWithoutHR(gpuTimeOpaque[i]->Map(0, nullptr, reinterpret_cast<void**>(&pRes)));
		uint64_t t1 = pRes[0];
		uint64_t t2 = pRes[1];
		gpuTimeOpaque[i]->Unmap(0, nullptr);

		gpuTimeOpaqueMs += static_cast<double>(t2 - t1) / static_cast<double>(GraphicManager::Instance().GetGpuFreq()) * 1000.0;

		// cutout time
		LogIfFailedWithoutHR(gpuTimeCutout[i]->Map(0, nullptr, reinterpret_cast<void**>(&pRes)));
		t1 = pRes[0];
		t2 = pRes[1];
		gpuTimeCutout[i]->Unmap(0, nullptr);

		gpuTimeCutoutMs += static_cast<double>(t2 - t1) / static_cast<double>(GraphicManager::Instance().GetGpuFreq()) * 1000.0;
	}
	totalGpuMs += gpuTimeCutoutMs + gpuTimeOpaqueMs;
}

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

		cout << "\n-------------- GPU Profile(ms) --------------\n";
		cout << "GPU Time: " << totalGpuMs << endl;
		cout << "Begin Frame (Clear Target) : " << gpuTimeMs[GpuTimeType::BeginFrame] << endl;
		cout << "Prepass Work (Depth) : " << gpuTimeMs[GpuTimeType::PrepassWork] << endl;
		cout << "Ray Tracing Shadow : " << gpuTimeMs[GpuTimeType::RayTracingShadow] << endl;
		cout << "Skybox : " << gpuTimeMs[GpuTimeType::SkyboxPass] << endl;
		cout << "Forward Transparent: " << gpuTimeMs[GpuTimeType::TransparentPass] << endl;
		cout << "EndFrame (MSAA Resolve) : " << gpuTimeMs[GpuTimeType::EndFrame] << endl;
		cout << "Forward Opaque: " << gpuTimeOpaqueMs << endl;
		cout << "Forward Cutout: " << gpuTimeCutoutMs << endl;

		cout << endl << endl;
        profileTime = 0.0;
	}

    lastTime = std::chrono::steady_clock::now();
}
