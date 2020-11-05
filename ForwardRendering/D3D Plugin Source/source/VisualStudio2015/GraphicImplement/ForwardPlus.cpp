#include "ForwardPlus.h"
#include "../GraphicManager.h"
#include "../ShaderManager.h"
#include "../MaterialManager.h"

void ForwardPlus::Init(int _maxPointLight)
{
	int w, h;
	GraphicManager::Instance().GetScreenSize(w, h);

	tileCountX = (int)ceil((float)w / tileSize);
	tileCountY = (int)ceil((float)h / tileSize);

	UINT totalSize = _maxPointLight * 4 + 4;
	totalSize *= tileCountX * tileCountY;
	pointLightTiles = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), totalSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	pointLightTileSrv.uav = ResourceManager::Instance().AddNativeTexture(GetUniqueID(), pointLightTiles->Resource(), TextureInfo(false, false, true, false, true, totalSize / 4, 0));
	pointLightTileSrv.srv = ResourceManager::Instance().AddNativeTexture(GetUniqueID(), pointLightTiles->Resource(), TextureInfo(false, false, false, false, true, totalSize / 4, 0));

	pointLightTilesTrans = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), totalSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	pointLightTransTileSrv.uav = ResourceManager::Instance().AddNativeTexture(GetUniqueID(), pointLightTilesTrans->Resource(), TextureInfo(false, false, true, false, true, totalSize / 4, 0));
	pointLightTransTileSrv.srv = ResourceManager::Instance().AddNativeTexture(GetUniqueID(), pointLightTilesTrans->Resource(), TextureInfo(false, false, false, false, true, totalSize / 4, 0));

	auto tileShader = ShaderManager::Instance().CompileShader(L"ForwardPlusTile.hlsl");
	if (tileShader != nullptr)
	{
		forwardPlusTileMat = MaterialManager::Instance().CreateComputeMat(tileShader);
	}
}

void ForwardPlus::Release()
{
	pointLightTiles.reset();
	pointLightTilesTrans.reset();
	forwardPlusTileMat.Release();
}

D3D12_GPU_DESCRIPTOR_HANDLE ForwardPlus::GetLightCullingUav()
{
	return ResourceManager::Instance().GetTexHandle(pointLightTileSrv.uav);
}

D3D12_GPU_DESCRIPTOR_HANDLE ForwardPlus::GetLightCullingSrv()
{
	return ResourceManager::Instance().GetTexHandle(pointLightTileSrv.srv);
}

D3D12_GPU_DESCRIPTOR_HANDLE ForwardPlus::GetLightCullingTransUav()
{
	return ResourceManager::Instance().GetTexHandle(pointLightTransTileSrv.uav);
}

D3D12_GPU_DESCRIPTOR_HANDLE ForwardPlus::GetLightCullingTransSrv()
{
	return ResourceManager::Instance().GetTexHandle(pointLightTransTileSrv.srv);
}

void ForwardPlus::GetTileCount(int& _x, int& _y)
{
	_x = tileCountX;
	_y = tileCountY;
}

ID3D12Resource* ForwardPlus::GetPointLightTileSrc()
{
	return pointLightTiles->Resource();
}

ID3D12Resource* ForwardPlus::GetPointLightTileTransSrc()
{
	return pointLightTilesTrans->Resource();
}

void ForwardPlus::TileLightCulling(D3D12_GPU_VIRTUAL_ADDRESS _pointLightGPU)
{
	// use pre gfx list
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
	auto _cmdList = GraphicManager::Instance().GetFrameResource()->mainGfxList;

	LogIfFailedWithoutHR(_cmdList->Reset(GraphicManager::Instance().GetFrameResource()->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery());

	if (!MaterialManager::Instance().SetComputePass(_cmdList, &forwardPlusTileMat))
	{
		return;
	}

	// set heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { ResourceManager::Instance().GetTexHeap(), ResourceManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// barriers
	D3D12_RESOURCE_BARRIER barriers[2];
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(pointLightTiles->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(pointLightTilesTrans->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_cmdList->ResourceBarrier(2, barriers);

	// set pso & root signature
	_cmdList->SetComputeRootDescriptorTable(0, GetLightCullingUav());
	_cmdList->SetComputeRootDescriptorTable(1, GetLightCullingTransUav());
	_cmdList->SetComputeRootConstantBufferView(2, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetComputeRootShaderResourceView(3, _pointLightGPU);
	_cmdList->SetComputeRootDescriptorTable(4, ResourceManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(5, ResourceManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());

	// compute work
	_cmdList->Dispatch(tileCountX, tileCountY, 1);

	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(pointLightTiles->Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(pointLightTilesTrans->Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(2, barriers);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::TileLightCulling]);
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}
