#include "GaussianBlur.h"
#include "../ShaderManager.h"
#include "../MaterialManager.h"
#include "../GraphicManager.h"
#include "../Formatter.h"

Material GaussianBlur::blurCompute;
BlurConstant GaussianBlur::blurConstantCPU;
BlurConstant GaussianBlur::prevBlurConstant;
unique_ptr<UploadBuffer<BlurConstant>> GaussianBlur::blurConstantGPU;
ComPtr<ID3D12Heap> GaussianBlur::blurTextureHeap;
UINT64 GaussianBlur::prevHeapSize;
DescriptorHeapData GaussianBlur::blurHeapData;

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

	// init descriptor heap ID
	blurHeapData.uniqueSrvID = GetUniqueID();
	blurHeapData.uniqueUavID = GetUniqueID();
}

void GaussianBlur::Release()
{
	blurCompute.Release();
	blurConstantGPU.reset();
	blurTextureHeap.Reset();
}

// ping-pong method
void GaussianBlur::BlurCompute(ID3D12GraphicsCommandList* _cmdList, BlurConstant _blurConst, ID3D12Resource* _src, D3D12_GPU_DESCRIPTOR_HANDLE _inputSrv, D3D12_GPU_DESCRIPTOR_HANDLE _inputUav)
{
	// assume descriptor heap is binded

	if (!MaterialManager::Instance().SetComputePass(_cmdList, &blurCompute))
	{
		return;
	}

	CalcBlurWeight(_blurConst);

	// get temp resource
	D3D12_RESOURCE_DESC desc = _src->GetDesc();
	desc.Format = Formatter::GetColorFormatFromTypeless(desc.Format);
	RequestBlurTextureHeap(desc);
	auto tmpSrc = CreateTempResource(desc);

	// upload constant
	UploadConstant(_blurConst, desc);

	// transition resource
	CD3DX12_RESOURCE_BARRIER barriers[2];
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Aliasing(nullptr, tmpSrc.Get());
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(_src, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(2, barriers);

	// horizontal pass
	auto frameIdx = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
	_cmdList->SetComputeRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(frameIdx));
	_cmdList->SetComputeRootConstantBufferView(1, blurConstantGPU->Resource()->GetGPUVirtualAddress());
	_cmdList->SetComputeRoot32BitConstant(2, 1, 0);
	_cmdList->SetComputeRootDescriptorTable(3, TextureManager::Instance().GetTexHandle(blurHeapData.uav));
	_cmdList->SetComputeRootDescriptorTable(4, _inputSrv);
	_cmdList->Dispatch((UINT)desc.Width / 8, desc.Height / 8, 1);

	// vertical pass
	_cmdList->SetComputeRoot32BitConstant(2, 0, 0);
	_cmdList->SetComputeRootDescriptorTable(3, _inputUav);
	_cmdList->SetComputeRootDescriptorTable(4, TextureManager::Instance().GetTexHandle(blurHeapData.srv));
	_cmdList->Dispatch((UINT)desc.Width / 8, desc.Height / 8, 1);
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

	blurTextureHeap.Reset();
	CD3DX12_HEAP_DESC heapDesc(heapSize, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES);
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreateHeap(&heapDesc, IID_PPV_ARGS(&blurTextureHeap)));
	prevHeapSize = heapSize;
}

void GaussianBlur::CalcBlurWeight(BlurConstant& _const)
{
	float sigma = _const.blurRadius / 2.0f;
	float twoSigma2 = 2.0f * sigma * sigma;
	float weightSum = 0.0f;
	int weightCount = 0;

	// calc gaussian sigma
	for (int i = -_const.blurRadius; i <= _const.blurRadius; i++)
	{
		float x = (float)i;

		if (i + _const.blurRadius < MAX_BLUR_WEIGHT)
			_const.blurWeight[i + _const.blurRadius] = expf(-x * x / twoSigma2);

		weightSum += _const.blurWeight[i + _const.blurRadius];
		weightCount++;
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (int i = 0; i < weightCount; i++)
	{
		_const.blurWeight[i] /= weightSum;
	}
}

void GaussianBlur::UploadConstant(BlurConstant& _blurConst, D3D12_RESOURCE_DESC _desc)
{
	_blurConst.invTargetSize.x = 1.0f / (float)_desc.Width;
	_blurConst.invTargetSize.y = 1.0f / (float)_desc.Height;

	blurConstantCPU = _blurConst;

	if (!(blurConstantCPU == prevBlurConstant))
	{
		blurConstantGPU->CopyData(0, blurConstantCPU);
	}

	prevBlurConstant = blurConstantCPU;
}

ComPtr<ID3D12Resource> GaussianBlur::CreateTempResource(D3D12_RESOURCE_DESC _desc)
{
	// create placed resource
	ComPtr<ID3D12Resource> tmpSrc;

	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreatePlacedResource(blurTextureHeap.Get()
		, 0
		, &_desc
		, D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		, nullptr
		, IID_PPV_ARGS(&tmpSrc)));

	// create srv
	blurHeapData.srv = TextureManager::Instance().UpdateNativeTexture(blurHeapData.uniqueSrvID, tmpSrc.Get(), TextureInfo());
	blurHeapData.uav = TextureManager::Instance().UpdateNativeTexture(blurHeapData.uniqueUavID, tmpSrc.Get(), TextureInfo(false, false, true, false, false));

	return tmpSrc;
}
