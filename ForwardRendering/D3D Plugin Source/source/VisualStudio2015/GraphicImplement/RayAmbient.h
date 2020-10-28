#pragma once
#include <d3d12.h>
#include "../Material.h"
#include "../TextureManager.h"
#include "../stdafx.h"
#include "../Camera.h"

class RayAmbient
{
public:
	void Init(ID3D12Resource* _ambientRT, ID3D12Resource* _noiseTex);
	void Release();
	void Trace(Camera* _targetCam, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU);

	int GetAmbientSrv();
	Material* GetMaterial();

private:
	D3D12_GPU_DESCRIPTOR_HANDLE GetAmbientUav();

	ID3D12Resource* ambientSrc;
	Material rtAmbientMat;
	DescriptorHeapData ambientHeapData;
	DescriptorHeapData noiseHeapData;
};