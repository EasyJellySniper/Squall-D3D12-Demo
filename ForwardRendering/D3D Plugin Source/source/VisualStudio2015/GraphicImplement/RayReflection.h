#pragma once
#include "../Material.h"
#include "../Camera.h"
#include "ForwardPlus.h"
#include "Skybox.h"
#include "../DefaultBuffer.h"

class RayReflection
{
public:
	void Init(ID3D12Resource* _rayReflection);
	void Release();
	void Trace(Camera* _targetCam, ForwardPlus* _forwardPlus, Skybox* _skybox, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU);
	Material* GetMaterial();
	DescriptorHeapData GetRayReflectionHeap();
	DescriptorHeapData GetTransRayReflectionHeap();

private:
	D3D12_GPU_DESCRIPTOR_HANDLE GetReflectionUav();
	D3D12_GPU_DESCRIPTOR_HANDLE GetTransReflectionUav();

	unique_ptr<DefaultBuffer> transRayReflection;
	ID3D12Resource* rayReflectionSrc;

	Material rayReflectionMat;
	DescriptorHeapData rayReflectoinSrv;
	DescriptorHeapData transRayReflectionHeap;
};