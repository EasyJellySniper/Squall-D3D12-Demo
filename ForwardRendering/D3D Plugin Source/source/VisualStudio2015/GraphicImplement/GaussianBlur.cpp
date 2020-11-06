#include "GaussianBlur.h"
#include "../ShaderManager.h"
#include "../MaterialManager.h"
#include "../GraphicManager.h"
#include "../Formatter.h"

Material GaussianBlur::blurCompute;
BlurConstant GaussianBlur::blurConstantCPU;
DescriptorHeapData GaussianBlur::blurHeapData;
ComPtr<ID3D12Resource> GaussianBlur::tmpSrc;

void GaussianBlur::Init()
{
	// init material
	Shader* shader = ShaderManager::Instance().CompileShader(L"GaussianBlurCompute.hlsl");
	if (shader != nullptr)
	{
		blurCompute = MaterialManager::Instance().CreateComputeMat(shader);
	}

	// init descriptor heap ID
	blurHeapData.AddSrv(nullptr, TextureInfo());
	blurHeapData.AddUav(nullptr, TextureInfo());
}

void GaussianBlur::Release()
{
	GraphicManager::Instance().WaitForGPU();

	blurCompute.Release();
	tmpSrc.Reset();
}

// ping-pong method
void GaussianBlur::BlurCompute(ID3D12GraphicsCommandList* _cmdList, BlurConstant _blurConst, ID3D12Resource* _src, D3D12_GPU_DESCRIPTOR_HANDLE _inputSrv, D3D12_GPU_DESCRIPTOR_HANDLE _inputUav)
{
	// assume descriptor heap is binded

	if (!MaterialManager::Instance().SetComputePass(_cmdList, &blurCompute))
	{
		return;
	}

	blurConstantCPU = _blurConst;
	CalcBlurWeight();

	// get temp resource
	D3D12_RESOURCE_DESC desc = _src->GetDesc();
	desc.Format = Formatter::GetColorFormatFromTypeless(desc.Format);
	CreateTempResource(ResourceManager::Instance().RequestHeap(desc), desc);

	// upload constant
	UploadConstant(desc);

	// transition resource
	CD3DX12_RESOURCE_BARRIER barriers[2];
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(_src, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(tmpSrc.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_cmdList->ResourceBarrier(2, barriers);

	// horizontal pass
	auto frameIdx = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
	_cmdList->SetComputeRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU());
	_cmdList->SetComputeRootDescriptorTable(1, ResourceManager::Instance().GetTexHandle(blurHeapData.Uav()));
	_cmdList->SetComputeRoot32BitConstants(2, sizeof(blurConstantCPU) / 4, &blurConstantCPU, 0);
	_cmdList->SetComputeRoot32BitConstant(3, 1, 0);
	_cmdList->SetComputeRootDescriptorTable(4, _inputSrv);
	_cmdList->SetComputeRootDescriptorTable(5, ResourceManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(6, ResourceManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());

	UINT computeKernel = 8;
	_cmdList->Dispatch(((UINT)desc.Width + computeKernel) / computeKernel, (desc.Height + computeKernel) / computeKernel, 1);

	// vertical pass
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(_src, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(tmpSrc.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(2, barriers);

	_cmdList->SetComputeRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU());
	_cmdList->SetComputeRootDescriptorTable(1, _inputUav);
	_cmdList->SetComputeRoot32BitConstants(2, sizeof(blurConstantCPU) / 4, &blurConstantCPU, 0);
	_cmdList->SetComputeRoot32BitConstant(3, 0, 0);
	_cmdList->SetComputeRootDescriptorTable(4, ResourceManager::Instance().GetTexHandle(blurHeapData.Srv()));
	_cmdList->SetComputeRootDescriptorTable(5, ResourceManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(6, ResourceManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->Dispatch((UINT)desc.Width / 8, desc.Height / 8, 1);
}

void GaussianBlur::CalcBlurWeight()
{
	float sigma = blurConstantCPU.blurRadius / 2.0f;
	float twoSigma2 = 2.0f * sigma * sigma;
	float weightSum = 0.0f;
	int weightCount = 0;

	// calc gaussian sigma
	for (int i = -blurConstantCPU.blurRadius; i <= blurConstantCPU.blurRadius; i++)
	{
		float x = (float)i;

		if (i + blurConstantCPU.blurRadius < MAX_BLUR_WEIGHT)
			blurConstantCPU.blurWeight[i + blurConstantCPU.blurRadius] = expf(-x * x / twoSigma2);

		weightSum += blurConstantCPU.blurWeight[i + blurConstantCPU.blurRadius];
		weightCount++;
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (int i = 0; i < weightCount; i++)
	{
		blurConstantCPU.blurWeight[i] /= weightSum;
	}
}

void GaussianBlur::UploadConstant(D3D12_RESOURCE_DESC _desc)
{
	blurConstantCPU.targetSize.x = (float)_desc.Width;
	blurConstantCPU.targetSize.y = (float)_desc.Height;
	blurConstantCPU.targetSize.z = 1.0f / (float)_desc.Width;
	blurConstantCPU.targetSize.w = 1.0f / (float)_desc.Height;
}

void GaussianBlur::CreateTempResource(ID3D12Heap* _heap, D3D12_RESOURCE_DESC _desc)
{
	// create placed resource
	tmpSrc.Reset();
	LogIfFailedWithoutHR(GraphicManager::Instance().GetDevice()->CreatePlacedResource(_heap
		, 0
		, &_desc
		, D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		, nullptr
		, IID_PPV_ARGS(tmpSrc.GetAddressOf())));

	// create srv
	blurHeapData.UpdateSrv(tmpSrc.Get(), TextureInfo());
	blurHeapData.UpdateUav(tmpSrc.Get(), TextureInfo(false, false, true, false, false));
}
