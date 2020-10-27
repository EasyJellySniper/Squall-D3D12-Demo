#pragma once
#include <d3d12.h>
#include "../Material.h"
#include "../TextureManager.h"
#include "../stdafx.h"

class RayAmbient
{
public:
	void Init(ID3D12Resource* _ambientRT);
	void Release();

	D3D12_CPU_DESCRIPTOR_HANDLE GetAmbientRtv();
	void Clear(ID3D12GraphicsCommandList *_cmdList);
	int GetAmbientSrv();

private:
	D3D12_GPU_DESCRIPTOR_HANDLE GetAmbientUav();

	unique_ptr<Texture> ambientRT;
	ID3D12Resource* ambientSrc;
	Material rtAmbientMat;
	DescriptorHeapData ambientHeapData;
};