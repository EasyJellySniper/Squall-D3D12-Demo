#include "RayAmbient.h"
#include "../stdafx.h"
#include "../ShaderManager.h"
#include "../MaterialManager.h"
#include "../GraphicManager.h"
#include "../RayTracingManager.h"

void RayAmbient::Init(ID3D12Resource* _ambientRT)
{
	ambientSrc = _ambientRT;

	// create uav/srv
	ambientHeapData.uav = TextureManager::Instance().AddNativeTexture(GetUniqueID(), ambientSrc, TextureInfo(true, false, true, false, false));
	ambientHeapData.srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), ambientSrc, TextureInfo(true, false, false, false, false));

	// create material
	Shader* rtAmbient = ShaderManager::Instance().CompileShader(L"RayTracingAmbient.hlsl");
	if (rtAmbient != nullptr)
	{
		rtAmbientMat = MaterialManager::Instance().CreateRayTracingMat(rtAmbient);
	}
}

void RayAmbient::Release()
{
	rtAmbientMat.Release();
}

void RayAmbient::Trace(Camera* _targetCam, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU)
{
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
	auto _cmdList = GraphicManager::Instance().GetFrameResource()->mainGfxList;

	LogIfFailedWithoutHR(_cmdList->Reset(GraphicManager::Instance().GetFrameResource()->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery());

	auto dxrCmd = GraphicManager::Instance().GetDxrList();
	if (!MaterialManager::Instance().SetRayTracingPass(dxrCmd, &rtAmbientMat))
	{
		return;
	}

	// copy hit group
	MaterialManager::Instance().CopyHitGroupIdentifier(GetMaterial(), HitGroupType::Ambient);

	// bind heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(), TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// barriers before tracing
	D3D12_RESOURCE_BARRIER barriers[3];
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetRtvSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(ambientSrc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_cmdList->ResourceBarrier(3, barriers);

	// set roots
	_cmdList->SetComputeRootDescriptorTable(0, GetAmbientUav());
	_cmdList->SetComputeRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetComputeRootShaderResourceView(2, RayTracingManager::Instance().GetTopLevelAS()->GetGPUVirtualAddress());
	_cmdList->SetComputeRootShaderResourceView(3, _dirLightGPU);
	_cmdList->SetComputeRootDescriptorTable(4, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(5, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(6, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(7, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootShaderResourceView(8, RayTracingManager::Instance().GetSubMeshInfoGPU());

	// prepare dispatch desc
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = rtAmbientMat.GetDispatchRayDesc((UINT)ambientSrc->GetDesc().Width, ambientSrc->GetDesc().Height);

	// setup hit group table
	auto hitGroup = MaterialManager::Instance().GetHitGroupGPU(HitGroupType::Ambient);
	dispatchDesc.HitGroupTable.StartAddress = hitGroup->Resource()->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = hitGroup->Resource()->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = hitGroup->Stride();

	// dispatch rays
	dxrCmd->DispatchRays(&dispatchDesc);

	// barriers after tracing
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetRtvSrc(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(ambientSrc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(3, barriers);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::RayTracingAmbient]);
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}

int RayAmbient::GetAmbientSrv()
{
	return ambientHeapData.srv;
}

Material* RayAmbient::GetMaterial()
{
	return &rtAmbientMat;
}

D3D12_GPU_DESCRIPTOR_HANDLE RayAmbient::GetAmbientUav()
{
	return TextureManager::Instance().GetTexHandle(ambientHeapData.uav);
}
