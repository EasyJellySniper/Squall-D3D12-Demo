#include "RayTracingManager.h"
#include "GraphicManager.h"
#include "MeshManager.h"
#include "RendererManager.h"
#include "MaterialManager.h"

void RayTracingManager::Release()
{
	subMeshInfo.reset();
	allTopAS.Release();
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
	int numTopASInstance = (int)allTopAS.instanceDescs.size();
	subMeshInfo = make_unique<UploadBuffer<SubMesh>>(GraphicManager::Instance().GetDevice(), numTopASInstance, false);

	int count = 0;
	auto renderers = RendererManager::Instance().GetRenderers();

	// sub mesh info
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
	return allTopAS.topLevelAS->Resource();
}

D3D12_GPU_VIRTUAL_ADDRESS RayTracingManager::GetSubMeshInfoGPU()
{
	return subMeshInfo->Resource()->GetGPUVirtualAddress();
}

void RayTracingManager::UpdateTopAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList)
{
	CollectRayTracingDesc(allTopAS.instanceDescs, RayInstanceType::All);

	// create dynamic top as (prefer fast build)
	CreateTopASWork(_dxrList, allTopAS, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD);
}

void RayTracingManager::CreateTopAccelerationStructure(ID3D12GraphicsCommandList5* _dxrList)
{
	// collect instance descs
	CollectRayTracingDesc(allTopAS.instanceDescs, RayInstanceType::All);

	// create all top as (prefer fast trace)
	CreateTopASWork(_dxrList, allTopAS, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD);
}

void RayTracingManager::CollectRayTracingDesc(vector<D3D12_RAYTRACING_INSTANCE_DESC>& _input, RayInstanceType _type)
{
	auto renderers = RendererManager::Instance().GetRenderers();

	// prepare ray tracing instance desc
	_input.clear();
	for (auto& r : renderers)
	{
		if (_type == RayInstanceType::Static && r->IsDynamic())
		{
			// strip dynamic renderer if we want static
			continue;
		}

		if (_type == RayInstanceType::Dynamic && !r->IsDynamic())
		{
			// strip static renderer if we want dynamic
			continue;
		}

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

			_input.push_back(rtInstancedesc);
		}
	}
}

void RayTracingManager::CreateTopASWork(ID3D12GraphicsCommandList5* _dxrList, TopLevelAS &_topLevelAS, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS _buildFlag)
{
	// prepare top level AS build
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = _buildFlag;
	topLevelInputs.NumDescs = (UINT)_topLevelAS.instanceDescs.size();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	GraphicManager::Instance().GetDxrDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);

	if (topLevelPrebuildInfo.ResultDataMaxSizeInBytes == 0)
	{
		LogMessage(L"[SqGraphic Error]: Create Top Acc Struct Failed.");
		return;
	}

	// create scratch & AS
	if (_topLevelAS.scratchTop == nullptr)
		_topLevelAS.scratchTop = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), topLevelPrebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	if (_topLevelAS.topLevelAS == nullptr)
		_topLevelAS.topLevelAS = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), topLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	// create upload buffer
	UINT bufferSize = static_cast<UINT>(_topLevelAS.instanceDescs.size() * sizeof(_topLevelAS.instanceDescs[0]));

	if (_topLevelAS.rayTracingInstance == nullptr)
		_topLevelAS.rayTracingInstance = make_unique<UploadBufferAny>(GraphicManager::Instance().GetDevice(), (UINT)_topLevelAS.instanceDescs.size(), false, bufferSize);

	_topLevelAS.rayTracingInstance->CopyData(0, _topLevelAS.instanceDescs.data());
	_topLevelAS.resultDataMaxSizeInBytes = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;

	// fill descs
	topLevelBuildDesc.DestAccelerationStructureData = _topLevelAS.topLevelAS->Resource()->GetGPUVirtualAddress();
	topLevelInputs.InstanceDescs = _topLevelAS.rayTracingInstance->Resource()->GetGPUVirtualAddress();
	topLevelBuildDesc.ScratchAccelerationStructureData = _topLevelAS.scratchTop->Resource()->GetGPUVirtualAddress();

	// Build acceleration structure.
	_dxrList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
}
