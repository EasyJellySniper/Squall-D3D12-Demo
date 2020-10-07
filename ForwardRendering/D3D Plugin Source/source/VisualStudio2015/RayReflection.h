#pragma once
#include "Material.h"
#include "Camera.h"
#include "ForwardPlus.h"
#include "Skybox.h"

class RayReflection
{
public:
	void Init(ID3D12Resource* _rayReflection);
	void Release();
	void Trace(Camera* _targetCam, ForwardPlus* _forwardPlus, Skybox* _skybox, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU, D3D12_GPU_VIRTUAL_ADDRESS _pointLightGPU);
	Material* GetMaterial();

private:
	D3D12_GPU_DESCRIPTOR_HANDLE GetReflectionUav();

	ID3D12Resource* rayReflectionSrc;
	Material rayReflectionMat;
	DescriptorHeapData rayReflectoinSrv;
};