#pragma once
#include "../Material.h"
#include "../ResourceManager.h"
#include "../UploadBuffer.h"

struct FXAAConstant
{
	bool operator!=(FXAAConstant& _lhs)
	{
		return targetSize.x != _lhs.targetSize.x;
	}

	XMFLOAT4 targetSize;
};

class FXAA
{
public:
	static void Init();
	static void Release();
	static void FXAACompute(ID3D12GraphicsCommandList* _cmdList, ID3D12Resource* _src, FXAAConstant _const, D3D12_GPU_DESCRIPTOR_HANDLE _outUav, D3D12_GPU_DESCRIPTOR_HANDLE _inSrv);

private:
	static void UploadConstant(D3D12_RESOURCE_DESC _desc);
	static void CreateTempResource(ID3D12Heap* _heap, D3D12_RESOURCE_DESC _desc);

	static Material fxaaComputeMat;
	static ComPtr<ID3D12Resource> tmpSrc;
	static DescriptorHeapData fxaaHeapData;

	static FXAAConstant fxaaConstantCPU;
	static FXAAConstant prevFxaaConstant;
	static unique_ptr<UploadBuffer<FXAAConstant>> fxaaConstantGPU;
};