#include "RayTracingManager.h"
#include "GraphicManager.h"
#include "MeshManager.h"
#include "RendererManager.h"

void RayTracingManager::Release()
{
	scratchTop.reset();
	topLevelAS.reset();
	rayTracingInstance.reset();
}

void RayTracingManager::InitRayTracingInstance()
{
	GraphicManager::Instance().ResetCreationList();
	auto dxrCmd = GraphicManager::Instance().GetDxrList();

	// build bottom AS
	MeshManager::Instance().CreateBottomAccelerationStructure(dxrCmd);

	// build top AS
	CreateTopAccelerationStructure(dxrCmd);

	GraphicManager::Instance().ExecuteCreationList();
	GraphicManager::Instance().WaitForGPU();

	// release temporary resources
	scratchTop.reset();
	rayTracingInstance.reset();
	MeshManager::Instance().ReleaseScratch();
}

ID3D12Resource* RayTracingManager::GetTopLevelAS()
{
	return topLevelAS->Resource();
}

void RayTracingManager::CreateTopAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList)
{
	auto renderers = RendererManager::Instance().GetRenderers();

	// prepare top level AS build
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	topLevelInputs.NumDescs = (UINT)renderers.size();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	GraphicManager::Instance().GetDxrDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);

	if (topLevelPrebuildInfo.ResultDataMaxSizeInBytes == 0)
	{
		LogMessage(L"[SqGraphic Error]: Create Top Acc Struct Failed.");
		return;
	}

	// create scratch & AS
	scratchTop = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), topLevelPrebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	topLevelAS = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), topLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	// prepare ray tracing instance desc
	vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;
	instanceDescs.resize(renderers.size());

	int i = 0;
	for (auto& r : renderers)
	{
		instanceDescs[i] = {};
		instanceDescs[i].InstanceMask = 1;
		instanceDescs[i].AccelerationStructure = r->GetMesh()->GetBottomAS()->GetGPUVirtualAddress();
		instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;	// unity use CCW
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDescs[i].Transform), XMLoadFloat4x4(&r->GetWorld()));

		i++;
	}

	// create upload buffer
	UINT bufferSize = static_cast<UINT>(instanceDescs.size() * sizeof(instanceDescs[0]));
	rayTracingInstance = make_unique<UploadBufferAny>(GraphicManager::Instance().GetDevice(), (UINT)renderers.size(), false, bufferSize);
	rayTracingInstance->CopyData(0, instanceDescs.data());

	// fill descs
	topLevelBuildDesc.DestAccelerationStructureData = topLevelAS->Resource()->GetGPUVirtualAddress();
	topLevelInputs.InstanceDescs = rayTracingInstance->Resource()->GetGPUVirtualAddress();
	topLevelBuildDesc.ScratchAccelerationStructureData = scratchTop->Resource()->GetGPUVirtualAddress();

	// Build acceleration structure.
	_dxrList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
}
