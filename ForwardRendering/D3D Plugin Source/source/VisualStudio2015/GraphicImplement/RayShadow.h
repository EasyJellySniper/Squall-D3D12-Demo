#pragma once
#include "../Material.h"
#include "../Texture.h"
#include "../DefaultBuffer.h"
#include "../Camera.h"
#include "ForwardPlus.h"
#include "../ResourceManager.h"

struct RayShadowData
{
	int collectShadowID;
	int collectTransShadowID;
	int pcfKernel;
	int rtShadowSrv;
};

class RayShadow
{
public:
	void Init(void* _collectShadows);
	void Relesae();

	void Clear(ID3D12GraphicsCommandList* _cmdList);
	void RayTracingShadow(Camera* _targetCam, ForwardPlus *_forwardPlus, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU, D3D12_GPU_VIRTUAL_ADDRESS _pointLightGPU);
	void CollectRayShadow(Camera* _targetCam);
	void SetPCFKernel(int _kernel);
	RayShadowData GetRayShadowData();

	Material* GetMaterial();
	ID3D12Resource* GetRayShadowSrc();
	ID3D12Resource* GetTransRayShadowSrc();
	int GetShadowIndex();
	ID3D12Resource* GetCollectShadowSrc();
	ID3D12Resource* GetCollectTransShadowSrc();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCollectShadowRtv();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCollectTransShadowRtv();
	D3D12_GPU_DESCRIPTOR_HANDLE GetRTShadowUav();
	D3D12_GPU_DESCRIPTOR_HANDLE GetRTShadowTransUav();

private:
	unique_ptr<Texture> collectShadow;
	unique_ptr<Texture> collectShadowTrans;
	unique_ptr<DefaultBuffer> transShadowSrc;
	unique_ptr<DefaultBuffer> rayTracingShadow;
	unique_ptr<DefaultBuffer> rayTracingShadowTrans;

	// shadow material
	Material collectRayShadowMat;
	DescriptorHeapData collectShadowSrv;
	DescriptorHeapData collectTransShadowSrv;
	int pcfKernel;

	// ray tracing material
	Material rtShadowMat;
	DescriptorHeapData rtShadowSrv;
	DescriptorHeapData rtShadowTransSrv;
};