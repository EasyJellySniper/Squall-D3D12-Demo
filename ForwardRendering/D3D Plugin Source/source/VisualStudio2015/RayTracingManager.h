#include <d3d12.h>
#include "DefaultBuffer.h"
#include "UploadBuffer.h"
#include "stdafx.h"

#pragma once

class RayTracingManager
{
public:
	RayTracingManager(const RayTracingManager&) = delete;
	RayTracingManager(RayTracingManager&&) = delete;
	RayTracingManager& operator=(const RayTracingManager&) = delete;
	RayTracingManager& operator=(RayTracingManager&&) = delete;

	static RayTracingManager& Instance()
	{
		static RayTracingManager instance;
		return instance;
	}

	RayTracingManager() {}
	~RayTracingManager() {}

	void Release();
	void BuildMeshRayTracing();

private:
	void CreateTopAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList);

	unique_ptr<DefaultBuffer> scratchTop;
	unique_ptr<DefaultBuffer> topLevelAS;
	unique_ptr<UploadBufferAny> rayTracingInstance;
};