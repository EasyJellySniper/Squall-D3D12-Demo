#include "RayTracingManager.h"
#include "GraphicManager.h"
#include "MeshManager.h"
#include "RendererManager.h"
#include "MaterialManager.h"

void RayTracingManager::Release()
{
	scratchTop.reset();
	topLevelAS.reset();
	rayTracingInstance.reset();
	subMeshInfo.reset();
	rayInstanceDescs.clear();
}

void RayTracingManager::InitRayTracingInstance()
{
	GraphicManager::Instance().ResetCreationList();
	auto dxrCmd = GraphicManager::Instance().GetDxrList();

	// build bottom AS
	MeshManager::Instance().CreateBottomAccelerationStructure(dxrCmd);

	// build top AS
	CreateTopAccelerationStructure(dxrCmd);
	CreateSubMeshInfoForTopAS();

	GraphicManager::Instance().ExecuteCreationList();
	GraphicManager::Instance().WaitForGPU();

	// release temporary resources
	MeshManager::Instance().ReleaseScratch();
}

void RayTracingManager::CreateSubMeshInfoForTopAS()
{
	// create enough buffer
	int numTopASInstance = (int)rayInstanceDescs.size();
	subMeshInfo = make_unique<UploadBuffer<SubMesh>>(GraphicManager::Instance().GetDevice(), numTopASInstance, false);

	int count = 0;
	auto renderers = RendererManager::Instance().GetRenderers();
	for (auto& r : renderers)
	{
		for (int i = 0; i < r->GetNumMaterials(); i++)
		{
			SubMesh sm = r->GetMesh()->GetSubMesh(i);
			subMeshInfo->CopyData(count++, sm);
		}
	}
}

ID3D12Resource* RayTracingManager::GetTopLevelAS()
{
	return topLevelAS->Resource();
}

D3D12_GPU_VIRTUAL_ADDRESS RayTracingManager::GetSubMeshInfoGPU()
{
	return subMeshInfo->Resource()->GetGPUVirtualAddress();
}

void RayTracingManager::UpdateTopAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList)
{
	auto renderers = RendererManager::Instance().GetRenderers();

	// update by rebuliding list
	// use fast build for updating
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;

	// update ray tracing desc transform
	int index = 0;
	for (auto& r : renderers)
	{
		for (int i = 0; i < r->GetNumMaterials(); i++)
		{
			XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(rayInstanceDescs[index++].Transform), XMLoadFloat4x4(&r->GetWorld()));
		}
	}

	// upload ray instance data & build
	// we don't need to reallocate buffer
	rayTracingInstance->CopyData(0, rayInstanceDescs.data());
	_dxrList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
}

void RayTracingManager::CreateTopAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList)
{
	// prepare top level AS build
	topLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	CollectRayTracingDesc();

	topLevelInputs.NumDescs = (UINT)rayInstanceDescs.size();

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

	// create upload buffer
	UINT bufferSize = static_cast<UINT>(rayInstanceDescs.size() * sizeof(rayInstanceDescs[0]));
	rayTracingInstance = make_unique<UploadBufferAny>(GraphicManager::Instance().GetDevice(), (UINT)rayInstanceDescs.size(), false, bufferSize);
	rayTracingInstance->CopyData(0, rayInstanceDescs.data());

	// fill descs
	topLevelBuildDesc.DestAccelerationStructureData = topLevelAS->Resource()->GetGPUVirtualAddress();
	topLevelInputs.InstanceDescs = rayTracingInstance->Resource()->GetGPUVirtualAddress();
	topLevelBuildDesc.ScratchAccelerationStructureData = scratchTop->Resource()->GetGPUVirtualAddress();

	// Build acceleration structure.
	_dxrList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
}

void RayTracingManager::CollectRayTracingDesc()
{
	auto renderers = RendererManager::Instance().GetRenderers();

	// prepare ray tracing instance desc
	rayInstanceDescs.clear();
	for (auto& r : renderers)
	{
		// build all submesh use by the renderer
		for (int i = 0; i < r->GetNumMaterials(); i++)
		{
			D3D12_RAYTRACING_INSTANCE_DESC rtInstancedesc;

			rtInstancedesc = {};
			rtInstancedesc.InstanceMask = 1;
			rtInstancedesc.AccelerationStructure = r->GetMesh()->GetBottomAS(i)->GetGPUVirtualAddress();
			rtInstancedesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;	// unity use CCW

			// setup material index in hitgroup table
			rtInstancedesc.InstanceContributionToHitGroupIndex = MaterialManager::Instance().GetMatIndexFromID(r->GetMaterial(i)->GetInstanceID());

			// setup instance id by using vertex buffer SRV
			rtInstancedesc.InstanceID = r->GetMesh()->GetVertexSrv();

			// non-opaque flags, force transparent object use any-hit shader
			if (r->GetMaterial(i)->GetRenderQueue() > RenderQueue::OpaqueLast)
			{
				rtInstancedesc.Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
			}

			// transform to world space
			XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(rtInstancedesc.Transform), XMLoadFloat4x4(&r->GetWorld()));

			rayInstanceDescs.push_back(rtInstancedesc);
		}
	}
}
