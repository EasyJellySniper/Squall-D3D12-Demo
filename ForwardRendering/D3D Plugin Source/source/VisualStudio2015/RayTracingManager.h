#pragma once
#include <d3d12.h>
#include "DefaultBuffer.h"
#include "UploadBuffer.h"
#include "stdafx.h"
#include "Mesh.h"
#include "Renderer.h"

struct TopLevelAS
{
	TopLevelAS()
	{
		scratchTop = nullptr;
		topLevelAS = nullptr;
		rayTracingInstance = nullptr;
		resultDataMaxSizeInBytes = 0;
	}

	void Release() 
	{
		instanceDescs.clear();
		rendererCache.clear();
		scratchTop.reset();
		topLevelAS.reset();
		rayTracingInstance.reset();
	}

	vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;
	vector<Renderer*> rendererCache;
	unique_ptr<DefaultBuffer> scratchTop;
	unique_ptr<DefaultBuffer> topLevelAS;
	unique_ptr<UploadBufferAny> rayTracingInstance;
	UINT64 resultDataMaxSizeInBytes;
};

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
	void UpdateTopAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList);

	int GetTopLevelAsCount();

private:
	void CreateTopAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList);
	void CollectRayTracingDesc(TopLevelAS& _input);
	void CreateTopASWork(ID3D12GraphicsCommandList5* _dxrList, TopLevelAS &_topLevelAS, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS _buildFlag);

	TopLevelAS allTopAS;
	unique_ptr<UploadBuffer<SubMesh>> subMeshInfo;
};