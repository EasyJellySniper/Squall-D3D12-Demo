#include "GaussianBlur.h"
#include "../ShaderManager.h"
#include "../MaterialManager.h"
#include "../GraphicManager.h"

Material GaussianBlur::blurCompute;
BlurConstant GaussianBlur::blurConstantCPU;
unique_ptr<UploadBuffer<BlurConstant>> GaussianBlur::blurConstantGPU;
ComPtr<ID3D12Heap> GaussianBlur::blurTextureHeap;
UINT64 GaussianBlur::prevHeapSize;

void GaussianBlur::Init()
{
	prevHeapSize = 0;

	// init material
	Shader* shader = ShaderManager::Instance().CompileShader(L"GaussianBlurCompute.hlsl");
	if (shader != nullptr)
	{
		blurCompute = MaterialManager::Instance().CreateComputeMat(shader);
	}

	// init constant
	blurConstantGPU = make_unique<UploadBuffer<BlurConstant>>(GraphicManager::Instance().GetDevice(), 1, true);

	// init heap, default with 1024 ARGB32 size
	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1024, 1024, 1, 1);
	RequestBlurTextureHeap(textureDesc);
}

void GaussianBlur::Release()
{
	blurCompute.Release();
	blurConstantGPU.reset();
	blurTextureHeap.Reset();
}

void GaussianBlur::RequestBlurTextureHeap(D3D12_RESOURCE_DESC _desc)
{
	_desc.Alignment = D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;

	D3D12_RESOURCE_ALLOCATION_INFO info = GraphicManager::Instance().GetDevice()->GetResourceAllocationInfo(0, 1, &_desc);
	if (info.Alignment != D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT)
	{
		// If the alignment requested is not granted, then let D3D tell us
		// the alignment that needs to be used for these resources.
		_desc.Alignment = 0;
		info = GraphicManager::Instance().GetDevice()->GetResourceAllocationInfo(0, 1, &_desc);
	}

	const UINT64 heapSize = info.SizeInBytes;
	if (heapSize < prevHeapSize)
	{
		// have enough heap size, return
		return;
	}

	CD3DX12_HEAP_DESC heapDesc(heapSize, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES);
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateHeap(&heapDesc, IID_PPV_ARGS(&blurTextureHeap)));
	prevHeapSize = heapSize;
}
