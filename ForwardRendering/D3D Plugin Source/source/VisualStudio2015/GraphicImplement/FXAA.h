#pragma once
#include "../Material.h"
#include "../ResourceManager.h"
#include "../UploadBuffer.h"

struct FXAAConstant
{
	XMFLOAT4 targetSize;
};

class FXAA
{
public:
	static void Init();
	static void Release();
	static void FXAACompute(ID3D12GraphicsCommandList* _cmdList, ID3D12Resource* _src, FXAAConstant _const, D3D12_GPU_DESCRIPTOR_HANDLE _outUav);

private:
	static void UploadConstant(D3D12_RESOURCE_DESC _desc);
	static void CreateTempResource(ID3D12Heap* _heap, D3D12_RESOURCE_DESC _desc);
	static D3D12_GPU_DESCRIPTOR_HANDLE GetFxaaSrv();

	static Material fxaaComputeMat;
	static ComPtr<ID3D12Resource> tmpSrc;
	static DescriptorHeapData fxaaHeapData;

	static FXAAConstant fxaaConstantCPU;
};