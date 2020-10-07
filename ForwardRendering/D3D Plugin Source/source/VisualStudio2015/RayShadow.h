#pragma once
#include "Material.h"
#include "Texture.h"
#include "DefaultBuffer.h"
#include "Camera.h"
#include "ForwardPlus.h"
#include "TextureManager.h"

struct RayShadowData
{
	int collectShadowID;
	int pcfKernel;
	int collectShadowSampler;
	int rtShadowSrv;
};

class RayShadow
{
public:
	void Init(void* _collectShadows);
	void Relesae();
	void RayTracingShadow(Camera* _targetCam, ForwardPlus *_forwardPlus, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU, D3D12_GPU_VIRTUAL_ADDRESS _pointLightGPU);
	void CollectRayShadow(Camera* _targetCam);
	void SetPCFKernel(int _kernel);
	RayShadowData GetRayShadowData();

	Material* GetRayShadow();
	ID3D12Resource* GetRayShadowSrc();
	int GetShadowIndex();
	ID3D12Resource* GetCollectShadowSrc();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCollectShadowRtv();
	D3D12_GPU_DESCRIPTOR_HANDLE GetRTShadowUav();

private:
	unique_ptr<Texture> collectShadow;
	unique_ptr<DefaultBuffer> rayTracingShadow;

	// shadow material
	Material collectRayShadowMat;
	DescriptorHeapData collectShadowSrv;
	int pcfKernel;

	// ray tracing material
	Material rtShadowMat;
	DescriptorHeapData rtShadowSrv;
};