#pragma once
#include <d3d12.h>
#include "../Material.h"
#include "../TextureManager.h"
#include "../stdafx.h"
#include "../Camera.h"
#include "../UploadBuffer.h"
#include "../DefaultBuffer.h"

struct UniformVector
{
	XMFLOAT4 v;
};

struct AmbientConstant
{
	float diffuseDist;
	float diffuseFadeDist;
	float diffuseStrength;
	float diffuseCutoff;
	float occlusionDist;
	float occlusionFadeDist;
	float occlusionStrength;
	float occlusionCutoff;
	float noiseTiling;
	float blurDepthThres;
	float blurNormalThres;
	int sampleCount;
	int ambientNoiseIndex;
	int blurRadius;
};

class RayAmbient
{
public:
	void Init(ID3D12Resource* _ambientRT, ID3D12Resource* _noiseTex);
	void Release();
	void Trace(Camera* _targetCam, D3D12_GPU_VIRTUAL_ADDRESS _dirLightGPU);
	void UpdataAmbientData(AmbientConstant _ac);

	int GetAmbientSrv();
	int GetAmbientNoiseSrv();
	Material* GetMaterial();

private:
	static const int maxSampleCount = 64;

	void CreateResource();
	void AmbientRegionFade(ID3D12GraphicsCommandList *_cmdList);
	D3D12_GPU_DESCRIPTOR_HANDLE GetAmbientUav();
	D3D12_GPU_DESCRIPTOR_HANDLE GetAmbientSrvHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE GetHitDistanceUav();
	D3D12_GPU_DESCRIPTOR_HANDLE GetHitDistanceSrv();

	ID3D12Resource* ambientSrc;
	Material rtAmbientMat;
	Material ambientRegionFadeMat;

	DescriptorHeapData ambientHeapData;
	DescriptorHeapData noiseHeapData;
	DescriptorHeapData hitDistanceData;
	AmbientConstant ambientConst;

	UniformVector uniformVectorCPU[maxSampleCount];
	unique_ptr<UploadBuffer<UniformVector>> uniformVectorGPU;
	unique_ptr<UploadBuffer<AmbientConstant>> ambientConstantGPU;
	unique_ptr<DefaultBuffer> ambientHitDistance;
};