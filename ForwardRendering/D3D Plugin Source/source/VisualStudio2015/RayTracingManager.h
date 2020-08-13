#include <d3d12.h>
#include "DefaultBuffer.h"
#include "UploadBuffer.h"
#include "stdafx.h"
#include "Mesh.h"

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
	void InitRayTracingInstance();
	void CreateSubMeshInfoForTopAS();
	ID3D12Resource* GetTopLevelAS();
	D3D12_GPU_VIRTUAL_ADDRESS GetSubMeshInfoGPU();

private:
	void CreateTopAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList);

	unique_ptr<DefaultBuffer> scratchTop;
	unique_ptr<DefaultBuffer> topLevelAS;
	unique_ptr<UploadBufferAny> rayTracingInstance;
	unique_ptr<UploadBuffer<SubMesh>> subMeshInfo;
	vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;
};