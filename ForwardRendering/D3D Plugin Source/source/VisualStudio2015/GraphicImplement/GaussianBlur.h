#pragma once
#include "../Material.h"
#include "../stdafx.h"
#include "../UploadBuffer.h"

struct BlurConstant
{
	float _BlurWeight[15];
	XMFLOAT2 _InvTargetSize;
	float _DepthThreshold;
	float _NormalThreshold;
	int _BlurRadius;
	int _HorizontalBlur;
};

class GaussianBlur
{
public:
	static void Init();
	static void Release();

private:
	static void RequestBlurTextureHeap(D3D12_RESOURCE_DESC _desc);

	static Material blurCompute;
	static BlurConstant blurConstantCPU;
	static unique_ptr<UploadBuffer<BlurConstant>> blurConstantGPU;
	static ComPtr<ID3D12Heap> blurTextureHeap;
	static UINT64 prevHeapSize;
};