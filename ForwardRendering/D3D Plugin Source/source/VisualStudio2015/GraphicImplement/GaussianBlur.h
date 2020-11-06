#pragma once
#include "../Material.h"
#include "../stdafx.h"
#include "../UploadBuffer.h"
#include "../ResourceManager.h"

static const int MAX_BLUR_WEIGHT = 16;

struct BlurConstant
{
	bool operator!=(BlurConstant &_rhs)
	{
		return depthThreshold != _rhs.depthThreshold ||
			normalThreshold != _rhs.normalThreshold ||
			blurRadius != _rhs.blurRadius;
	}

	BlurConstant() {}

	BlurConstant(int _radius, float _depthThreshold, float _normalThreshold)
	{
		blurRadius = _radius;
		depthThreshold = _depthThreshold;
		normalThreshold = _normalThreshold;
	}

	XMFLOAT4 targetSize;
	float depthThreshold;
	float normalThreshold;
	int blurRadius;
	float padding;

	float blurWeight[MAX_BLUR_WEIGHT];
};

class GaussianBlur
{
public:
	static void Init();
	static void Release();
	static void BlurCompute(ID3D12GraphicsCommandList* _cmdList, BlurConstant _blurConst, ID3D12Resource* _src, D3D12_GPU_DESCRIPTOR_HANDLE _inputSrv, D3D12_GPU_DESCRIPTOR_HANDLE _inputUav);

private:
	static void CalcBlurWeight();
	static void UploadConstant(D3D12_RESOURCE_DESC _desc);
	static void CreateTempResource(ID3D12Heap* _heap, D3D12_RESOURCE_DESC _desc);

	static Material blurCompute;
	static BlurConstant blurConstantCPU;

	static DescriptorHeapData blurHeapData;
	static ComPtr<ID3D12Resource> tmpSrc;
};