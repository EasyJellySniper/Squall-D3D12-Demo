#include "RayAmbient.h"
#include "../stdafx.h"

void RayAmbient::Init(ID3D12Resource* _ambientRT)
{
	ambientSrc = _ambientRT;

	// create rtv for clear
	ambientRT = make_unique<Texture>(1, 0);
	ambientRT->InitRTV(ambientSrc, GetColorFormatFromTypeless(ambientSrc->GetDesc().Format), false);

	// create uav/srv
	ambientHeapData.uav = TextureManager::Instance().AddNativeTexture(GetUniqueID(), ambientSrc, TextureInfo(true, false, true, false, false));
	ambientHeapData.srv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), ambientSrc, TextureInfo(true, false, false, false, false));
}

void RayAmbient::Release()
{
	if (ambientRT != nullptr)
		ambientRT->Release();
	ambientRT.reset();

	rtAmbientMat.Release();
}

D3D12_CPU_DESCRIPTOR_HANDLE RayAmbient::GetAmbientRtv()
{
	return ambientRT->GetRtvCPU(0);
}

void RayAmbient::Clear(ID3D12GraphicsCommandList* _cmdList)
{
	// rgb for indirect light, a for occlusion
	FLOAT c[] = { 0,0,0,1 };
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(ambientSrc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET));
	_cmdList->ClearRenderTargetView(GetAmbientRtv(), c, 0, nullptr);
}

int RayAmbient::GetAmbientSrv()
{
	return ambientHeapData.srv;
}

D3D12_GPU_DESCRIPTOR_HANDLE RayAmbient::GetAmbientUav()
{
	return TextureManager::Instance().GetTexHandle(ambientHeapData.uav);
}
