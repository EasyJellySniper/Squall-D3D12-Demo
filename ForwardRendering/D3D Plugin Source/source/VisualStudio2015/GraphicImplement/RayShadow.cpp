#include "RayShadow.h"
#include "../ShaderManager.h"
#include "../MaterialManager.h"
#include "../GraphicManager.h"
#include "../RayTracingManager.h"

void RayShadow::Init(void* _collectShadows)
{
	if (_collectShadows == nullptr)
	{
		return;
	}

	ID3D12Resource* opaqueShadowSrc = (ID3D12Resource*)_collectShadows;

	auto desc = opaqueShadowSrc->GetDesc();
	DXGI_FORMAT shadowFormat = GetColorFormatFromTypeless(desc.Format);

	transShadowSrc = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), desc, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// register to texture manager
	collectShadow = make_unique<Texture>(1, 0);
	collectShadow->InitRTV(opaqueShadowSrc, shadowFormat, false);
	collectShadowSrv.srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), opaqueShadowSrc, TextureInfo(true, false, false, false, false));

	collectShadowTrans = make_unique<Texture>(1, 0);
	collectShadowTrans->InitRTV(transShadowSrc->Resource(), shadowFormat, false);
	collectTransShadowSrv.srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), transShadowSrc->Resource(), TextureInfo(true, false, false, false, false));

	// create collect shadow material
	Shader* collectRayShader = ShaderManager::Instance().CompileShader(L"CollectRayShadow.hlsl");
	if (collectRayShader != nullptr)
	{
		collectRayShadowMat = MaterialManager::Instance().CreatePostMat(collectRayShader, false, 1, &shadowFormat, DXGI_FORMAT_UNKNOWN);
	}

	collectShadowSrv.sampler = TextureManager::Instance().AddNativeSampler(TextureWrapMode::Clamp, TextureWrapMode::Clamp, TextureWrapMode::Clamp, 8, D3D12_FILTER_ANISOTROPIC);

	// create ray tracing shadow uav
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	int w, h;
	GraphicManager::Instance().GetScreenSize(w, h);
	desc.Width = w;
	desc.Height = h;
	rayTracingShadow = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	rayTracingShadowTrans = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// create ray tracing texture
	rtShadowSrv.uav = TextureManager::Instance().AddNativeTexture(GetUniqueID(), rayTracingShadow->Resource(), TextureInfo(false, false, true, false, false));
	rtShadowSrv.srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), rayTracingShadow->Resource(), TextureInfo());
	rtShadowTransSrv.uav = TextureManager::Instance().AddNativeTexture(GetUniqueID(), rayTracingShadowTrans->Resource(), TextureInfo(false, false, true, false, false));
	rtShadowTransSrv.srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), rayTracingShadowTrans->Resource(), TextureInfo());

	// create shader & material
	Shader* rtShadowShader = ShaderManager::Instance().CompileShader(L"RayTracingShadow.hlsl", nullptr);
	if (rtShadowShader != nullptr)
	{
		rtShadowMat = MaterialManager::Instance().CreateRayTracingMat(rtShadowShader);
	}
}

void RayShadow::Relesae()
{
	if (collectShadow != nullptr)
		collectShadow->Release();

	if (collectShadowTrans != nullptr)
		collectShadowTrans->Release();

	collectShadow.reset();
	collectShadowTrans.reset();
	transShadowSrc.reset();
	rayTracingShadow.reset();
	rayTracingShadowTrans.reset();
	collectRayShadowMat.Release();
	rtShadowMat.Release();
}

void RayShadow::RayTracingShadow(Camera* _targetCam, ForwardPlus* _forwardPlus, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU, D3D12_GPU_VIRTUAL_ADDRESS _pointLightGPU)
{
	// use pre gfx list
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
	auto _cmdList = GraphicManager::Instance().GetFrameResource()->mainGfxList;

	LogIfFailedWithoutHR(_cmdList->Reset(GraphicManager::Instance().GetFrameResource()->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery());

	auto dxrCmd = GraphicManager::Instance().GetDxrList();
	Material* mat = GetRayShadow();
	if (!MaterialManager::Instance().SetRayTracingPass(dxrCmd, mat))
	{
		return;
	}

	// bind root signature
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(), TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	UINT cbvSrvUavSize = GraphicManager::Instance().GetCbvSrvUavDesciptorSize();

	D3D12_RESOURCE_BARRIER barriers[6];
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetRtvSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetNormalSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(4, barriers);

	// set state
	_cmdList->SetComputeRootDescriptorTable(0, GetRTShadowUav());
	_cmdList->SetComputeRootDescriptorTable(1, GetRTShadowTransUav());
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

	// prepare dispatch desc
	auto rtShadowSrc = rayTracingShadow->Resource();
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = mat->GetDispatchRayDesc((UINT)rtShadowSrc->GetDesc().Width, rtShadowSrc->GetDesc().Height);

	// setup hit group table
	auto hitGroup = MaterialManager::Instance().GetHitGroupGPU(HitGroupType::Shadow);
	dispatchDesc.HitGroupTable.StartAddress = hitGroup->Resource()->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = hitGroup->Resource()->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = hitGroup->Stride();

	// dispatch rays
	dxrCmd->DispatchRays(&dispatchDesc);

	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetRtvSrc(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
	barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetNormalSrc(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
	barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(_forwardPlus->GetPointLightTileSrc(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(_forwardPlus->GetPointLightTileTransSrc(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(5, barriers);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::RayTracingShadow]);
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}

void RayShadow::CollectRayShadow(Camera* _targetCam)
{
	auto currFrameResource = GraphicManager::Instance().GetFrameResource();
	auto _cmdList = currFrameResource->mainGfxList;

	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery());

	if (!MaterialManager::Instance().SetGraphicPass(_cmdList, &collectRayShadowMat))
	{
		return;
	}

	// transition resource
	D3D12_RESOURCE_BARRIER collect[6];
	collect[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	collect[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	collect[2] = CD3DX12_RESOURCE_BARRIER::Transition(GetCollectShadowSrc(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
	collect[3] = CD3DX12_RESOURCE_BARRIER::Transition(GetCollectTransShadowSrc(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
	collect[4] = CD3DX12_RESOURCE_BARRIER::Transition(GetRayShadowSrc(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	collect[5] = CD3DX12_RESOURCE_BARRIER::Transition(GetTransRayShadowSrc(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(6, collect);

	// set heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap() , TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// set target
	_cmdList->OMSetRenderTargets(1, &GetCollectShadowRtv(), true, nullptr);

	// set viewport
	auto viewPort = _targetCam->GetViewPort();
	auto scissor = _targetCam->GetScissorRect();
	viewPort.Width = (FLOAT)GetCollectShadowSrc()->GetDesc().Width;
	viewPort.Height = (FLOAT)GetCollectShadowSrc()->GetDesc().Height;
	scissor.right = (LONG)viewPort.Width;
	scissor.bottom = (LONG)viewPort.Height;

	_cmdList->RSSetViewports(1, &viewPort);
	_cmdList->RSSetScissorRects(1, &scissor);
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// set material
	_cmdList->SetGraphicsRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(currFrameResource->currFrameIndex));
	_cmdList->SetGraphicsRootDescriptorTable(1, TextureManager::Instance().GetTexHandle(rtShadowSrv.srv));
	_cmdList->SetGraphicsRootDescriptorTable(2, _targetCam->GetDsvGPU());
	_cmdList->SetGraphicsRootDescriptorTable(3, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());

	// collect opaque
	_cmdList->DrawInstanced(6, 1, 0, 0);
	GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0]);

	// collect transparent
	_cmdList->OMSetRenderTargets(1, &GetCollectTransShadowRtv(), true, nullptr);
	_cmdList->SetGraphicsRootDescriptorTable(1, TextureManager::Instance().GetTexHandle(rtShadowTransSrv.srv));
	_cmdList->SetGraphicsRootDescriptorTable(2, _targetCam->GetTransDsvGPU());
	_cmdList->DrawInstanced(6, 1, 0, 0);
	GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0]);

	// transition back
	collect[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	collect[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	collect[2] = CD3DX12_RESOURCE_BARRIER::Transition(GetCollectShadowSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	collect[3] = CD3DX12_RESOURCE_BARRIER::Transition(GetCollectTransShadowSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	collect[4] = CD3DX12_RESOURCE_BARRIER::Transition(GetRayShadowSrc(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	collect[5] = CD3DX12_RESOURCE_BARRIER::Transition(GetTransRayShadowSrc(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_cmdList->ResourceBarrier(6, collect);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::CollectShadowMap]);
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}

void RayShadow::SetPCFKernel(int _kernel)
{
	pcfKernel = _kernel;
}

RayShadowData RayShadow::GetRayShadowData()
{
	RayShadowData rsd;
	rsd.collectShadowID = collectShadowSrv.srv;
	rsd.collectTransShadowID = collectTransShadowSrv.srv;
	rsd.collectShadowSampler = collectShadowSrv.sampler;
	rsd.pcfKernel = pcfKernel;
	rsd.rtShadowSrv = rtShadowSrv.srv;

	return rsd;
}

Material* RayShadow::GetRayShadow()
{
	return &rtShadowMat;
}

ID3D12Resource* RayShadow::GetRayShadowSrc()
{
	return rayTracingShadow->Resource();
}

ID3D12Resource* RayShadow::GetTransRayShadowSrc()
{
	return rayTracingShadowTrans->Resource();
}

int RayShadow::GetShadowIndex()
{
	return collectShadowSrv.srv;
}

ID3D12Resource* RayShadow::GetCollectShadowSrc()
{
	return collectShadow->GetRtvSrc(0);
}

ID3D12Resource* RayShadow::GetCollectTransShadowSrc()
{
	return transShadowSrc->Resource();
}

D3D12_CPU_DESCRIPTOR_HANDLE RayShadow::GetCollectShadowRtv()
{
	return collectShadow->GetRtvCPU(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE RayShadow::GetCollectTransShadowRtv()
{
	return collectShadowTrans->GetRtvCPU(0);
}

D3D12_GPU_DESCRIPTOR_HANDLE RayShadow::GetRTShadowUav()
{
	return TextureManager::Instance().GetTexHandle(rtShadowSrv.uav);
}

D3D12_GPU_DESCRIPTOR_HANDLE RayShadow::GetRTShadowTransUav()
{
	return TextureManager::Instance().GetTexHandle(rtShadowTransSrv.uav);
}
