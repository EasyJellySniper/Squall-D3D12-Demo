#include "GameTimerManager.h"
#include <iostream>
#include <iomanip>
using namespace std;
#include "GraphicManager.h"
#include "RayTracingManager.h"
#include "stdafx.h"

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


		LogIfFailedWithoutHR(mainDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK)
			, D3D12_HEAP_FLAG_NONE
			, &CD3DX12_RESOURCE_DESC::Buffer(2 * sizeof(uint64_t))
			, D3D12_RESOURCE_STATE_COPY_DEST
			, nullptr
			, IID_PPV_ARGS(&gpuTimeDepth[i])));
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
		gpuTimeDepth[i].Reset();
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
	gpuTimeDepthMs = 0;
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

		// depth time
		LogIfFailedWithoutHR(gpuTimeDepth[i]->Map(0, nullptr, reinterpret_cast<void**>(&pRes)));
		t1 = pRes[0];
		t2 = pRes[1];
		gpuTimeDepth[i]->Unmap(0, nullptr);

		gpuTimeDepthMs += static_cast<double>(t2 - t1) / static_cast<double>(GraphicManager::Instance().GetGpuFreq()) * 1000.0;
	}
	totalGpuMs += gpuTimeCutoutMs + gpuTimeOpaqueMs + gpuTimeDepthMs;
}

void GameTimerManager::PrintGameTime()
{
    chrono::steady_clock::time_point currTime = std::chrono::steady_clock::now();
    profileTime += std::chrono::duration_cast<std::chrono::milliseconds>(currTime - lastTime).count();

	if (profileTime > 1000.0)
	{
		cpuProfile = "";
		cpuProfile += "-------------- CPU Profile(ms) --------------\n";

		cpuProfile += "CPU Time: " + to_string_precision(gameTime.updateTime + gameTime.renderTime) + "\n";
		cpuProfile += "Wait GPU Fence: " + to_string_precision(gameTime.updateTime) + "\n";
		cpuProfile += "Culling Time: " + to_string_precision(gameTime.cullingTime) + "\n";
		cpuProfile += "Sorting Time: " + to_string_precision(gameTime.sortingTime) + "\n";
		cpuProfile += "Upload Time: " + to_string_precision(gameTime.uploadTime) + "\n";
		cpuProfile += "Render Time: " + to_string_precision(gameTime.renderTime) + "\n";

		int totalDrawCall = 0;
		for (int i = 0; i < GraphicManager::Instance().GetThreadCount() - 1; i++)
		{
			cpuProfile += "\tThread " + to_string_precision(i) + " Time:" + to_string_precision(gameTime.renderThreadTime[i]) + "\n";
			totalDrawCall += gameTime.batchCount[i];
		}

		cpuProfile += "Total DrawCall: " + to_string_precision(totalDrawCall) + "\n";
		cpuProfile += "Total ray tracing instance (Top Level): " + to_string_precision(RayTracingManager::Instance().GetTopLevelAsCount()) + "\n";

		gpuProfile = "";
		gpuProfile += "-------------- GPU Profile(ms) --------------\n";
		gpuProfile += "GPU Time: " + to_string_precision(totalGpuMs) + "\n";
		gpuProfile += "Begin Frame (Clear Target) : " + to_string_precision(gpuTimeMs[GpuTimeType::BeginFrame]) + "\n";
		gpuProfile += "Prepass Work (Depth) : " + to_string_precision(gpuTimeMs[GpuTimeType::PrepassWork] + gpuTimeDepthMs) + "\n";
		gpuProfile += "Update Top Level AS: " + to_string_precision(gpuTimeMs[GpuTimeType::UpdateTopLevelAS]) + "\n";
		gpuProfile += "Forward+ Light Culling: " + to_string_precision(gpuTimeMs[GpuTimeType::TileLightCulling]) + "\n";
		gpuProfile += "Collect Shadow Map: " + to_string_precision(gpuTimeMs[GpuTimeType::CollectShadowMap]) + "\n";
		gpuProfile += "Ray Tracing Shadow : " + to_string_precision(gpuTimeMs[GpuTimeType::RayTracingShadow]) + "\n";
		gpuProfile += "Ray Tracing Reflection: " + to_string_precision(gpuTimeMs[GpuTimeType::RayTracingReflection]) + "\n";
		gpuProfile += "Ray Tracing Ambient: " + to_string_precision(gpuTimeMs[GpuTimeType::RayTracingAmbient]) + "\n";
		gpuProfile += "Skybox : " + to_string_precision(gpuTimeMs[GpuTimeType::SkyboxPass]) + "\n";
		gpuProfile += "Forward Transparent: " + to_string_precision(gpuTimeMs[GpuTimeType::TransparentPass]) + "\n";
		gpuProfile += "EndFrame (MSAA Resolve) : " + to_string_precision(gpuTimeMs[GpuTimeType::EndFrame]) + "\n";
		gpuProfile += "Forward Opaque: " + to_string_precision(gpuTimeOpaqueMs) + "\n";
		gpuProfile += "Forward Cutout: " + to_string_precision(gpuTimeCutoutMs) + "\n";

		profileTime = 0.0;
	}

    lastTime = std::chrono::steady_clock::now();
}
