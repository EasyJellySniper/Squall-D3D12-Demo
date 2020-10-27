#pragma once
#include <d3d12.h>
#include "../Material.h"
#include "../TextureManager.h"
#include "../stdafx.h"
#include "../Camera.h"

class RayAmbient
{
public:
	void Init(ID3D12Resource* _ambientRT);
	void Release();
	void Trace(Camera* _targetCam, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU, int _ambientRange);

	D3D12_CPU_DESCRIPTOR_HANDLE GetAmbientRtv();
	void Clear(ID3D12GraphicsCommandList *_cmdList);
	int GetAmbientSrv();
	Material* GetMaterial();

private:
	D3D12_GPU_DESCRIPTOR_HANDLE GetAmbientUav();

	unique_ptr<Texture> ambientRT;
	ID3D12Resource* ambientSrc;
	Material rtAmbientMat;
	DescriptorHeapData ambientHeapData;
};