#include "RayReflection.h"
#include "../TextureManager.h"
#include "../ShaderManager.h"
#include "../MaterialManager.h"
#include "../GraphicManager.h"
#include "../RayTracingManager.h"
#include "GenerateMipmap.h"

void RayReflection::Init(ID3D12Resource* _rayReflection)
{
	rayReflectionSrc = _rayReflection;
	transRayReflection = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), rayReflectionSrc->GetDesc(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// register uav & srv & sampler
	rayReflectoinSrv.uav = TextureManager::Instance().AddNativeTexture(GetUniqueID(), _rayReflection, TextureInfo(true, false, true, false, false), true);
	rayReflectoinSrv.srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), _rayReflection, TextureInfo(true, false, false, false, false));

	transRayReflectionHeap.uav = TextureManager::Instance().AddNativeTexture(GetUniqueID(), transRayReflection->Resource(), TextureInfo(true, false, true, false, false), true);
	transRayReflectionHeap.srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), transRayReflection->Resource(), TextureInfo(true, false, false, false, false));

	// compile shader & material
	D3D_SHADER_MACRO rayReflectionMacro[] = { "_SPEC_GLOSS_MAP","1"
		,"_EMISSION","1"
		,"_NORMAL_MAP","1"
		,"_DETAIL_MAP","1"
		,"_DETAIL_NORMAL_MAP","1"
		,NULL,NULL };

	Shader* rayReflectionShader = ShaderManager::Instance().CompileShader(L"RayTracingReflection.hlsl", rayReflectionMacro);
	if (rayReflectionShader != nullptr)
	{
		rayReflectionMat = MaterialManager::Instance().CreateRayTracingMat(rayReflectionShader);
	}
}

void RayReflection::Release()
{
	rayReflectionMat.Release();
	transRayReflection.reset();
}

void RayReflection::Trace(Camera* _targetCam, ForwardPlus* _forwardPlus, Skybox* _skybox, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU, D3D12_GPU_VIRTUAL_ADDRESS _pointLightGPU)
{
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
	auto _cmdList = GraphicManager::Instance().GetFrameResource()->mainGfxList;

	LogIfFailedWithoutHR(_cmdList->Reset(GraphicManager::Instance().GetFrameResource()->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery());

	// bind heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(), TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// resource transition (normal/transnormal/depth/transdepth/uav
	D3D12_RESOURCE_BARRIER barriers[6];
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetRtvSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetNormalSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(rayReflectionSrc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition(transRayReflection->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_cmdList->ResourceBarrier(6, barriers);

	// set material
	_cmdList->SetComputeRootSignature(rayReflectionMat.GetRootSignature());
	_cmdList->SetComputeRootDescriptorTable(0, GetReflectionUav());
	_cmdList->SetComputeRootDescriptorTable(1, GetTransReflectionUav());
	_cmdList->SetComputeRootConstantBufferView(2, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetComputeRootDescriptorTable(3, _forwardPlus->GetLightCullingSrv());
	_cmdList->SetComputeRootDescriptorTable(4, _forwardPlus->GetLightCullingTransSrv());
	_cmdList->SetComputeRootShaderResourceView(5, RayTracingManager::Instance().GetTopLevelAS()->GetGPUVirtualAddress());
	_cmdList->SetComputeRootShaderResourceView(6, _dirLightGPU);
	_cmdList->SetComputeRootShaderResourceView(7, _pointLightGPU);
	_cmdList->SetComputeRootDescriptorTable(8, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(9, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(10, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(11, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootShaderResourceView(12, RayTracingManager::Instance().GetSubMeshInfoGPU());
	_cmdList->SetComputeRootDescriptorTable(13, _skybox->GetSkyboxTex());
	_cmdList->SetComputeRootDescriptorTable(14, _skybox->GetSkyboxSampler());

	// prepare dispatch desc
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = rayReflectionMat.GetDispatchRayDesc((UINT)rayReflectionSrc->GetDesc().Width, rayReflectionSrc->GetDesc().Height);

	// setup hit group table
	auto hitGroup = MaterialManager::Instance().GetHitGroupGPU(HitGroupType::Reflection);
	dispatchDesc.HitGroupTable.StartAddress = hitGroup->Resource()->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = hitGroup->Resource()->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = hitGroup->Stride();

	// dispatch rays
	auto dxrCmd = GraphicManager::Instance().GetDxrList();
	dxrCmd->SetPipelineState1(rayReflectionMat.GetDxcPSO());
	dxrCmd->DispatchRays(&dispatchDesc);

	// generate mipmap for reflection rt
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(rayReflectionSrc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(transRayReflection->Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(2, barriers);

	GenerateMipmap::Generate(_cmdList, rayReflectionSrc->GetDesc(), TextureManager::Instance().GetTexHandle(rayReflectoinSrv.srv), TextureManager::Instance().GetTexHandle(rayReflectoinSrv.uav));
	GenerateMipmap::Generate(_cmdList, rayReflectionSrc->GetDesc(), TextureManager::Instance().GetTexHandle(transRayReflectionHeap.srv), TextureManager::Instance().GetTexHandle(transRayReflectionHeap.uav));

	// resource transition back
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetRtvSrc(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetNormalSrc(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(rayReflectionSrc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition(transRayReflection->Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(6, barriers);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::RayTracingReflection]);
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}

Material* RayReflection::GetMaterial()
{
	return &rayReflectionMat;
}

DescriptorHeapData RayReflection::GetRayReflectionHeap()
{
	return rayReflectoinSrv;
}

DescriptorHeapData RayReflection::GetTransRayReflectionHeap()
{
	return transRayReflectionHeap;
}

D3D12_GPU_DESCRIPTOR_HANDLE RayReflection::GetReflectionUav()
{
	return TextureManager::Instance().GetTexHandle(rayReflectoinSrv.uav);
}

D3D12_GPU_DESCRIPTOR_HANDLE RayReflection::GetTransReflectionUav()
{
	return TextureManager::Instance().GetTexHandle(transRayReflectionHeap.uav);
}
