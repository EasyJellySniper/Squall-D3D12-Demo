#pragma once
#include <d3d12.h>
#include "../Material.h"
#include "../TextureManager.h"
#include "../stdafx.h"
#include "../Camera.h"
#include "../UploadBuffer.h"

struct UniformVector
{
	XMFLOAT4 v;
};

class RayAmbient
{
public:
	void Init(ID3D12Resource* _ambientRT, ID3D12Resource* _noiseTex);
	void Release();
	void Trace(Camera* _targetCam, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU);
	void UpdataSampleCount(int _count);

	int GetAmbientSrv();
	int GetAmbientNoiseSrv();
	Material* GetMaterial();

private:
	static const int maxSampleCount = 64;

	void CreateUniformVector();
	D3D12_GPU_DESCRIPTOR_HANDLE GetAmbientUav();

	ID3D12Resource* ambientSrc;
	Material rtAmbientMat;

	DescriptorHeapData ambientHeapData;
	DescriptorHeapData noiseHeapData;
	int sampleCount = 0;

	UniformVector uniformVectorCPU[maxSampleCount];
	unique_ptr<UploadBuffer<UniformVector>> uniformVectorGPU;
};