#include "GenerateMipmap.h"
#include "../ShaderManager.h"
#include "../MaterialManager.h"
#include "../GraphicManager.h"

Material GenerateMipmap::generateMat;

void GenerateMipmap::Init()
{
	Shader* mipMapShader = ShaderManager::Instance().CompileShader(L"GenerateMipmap.hlsl");
	if (mipMapShader != nullptr)
	{
		generateMat = MaterialManager::Instance().CreateComputeMat(mipMapShader);
	}
}

void GenerateMipmap::Release()
{
	generateMat.Release();
}

void GenerateMipmap::Generate(ID3D12GraphicsCommandList* _cmdList, D3D12_RESOURCE_DESC _srcDesc, D3D12_GPU_DESCRIPTOR_HANDLE _input, D3D12_GPU_DESCRIPTOR_HANDLE _outMip)
{
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;

	// assume we already set descriptor heaps
	//ID3D12DescriptorHeap* descriptorHeaps[] = { ResourceManager::Instance().GetTexHeap(), ResourceManager::Instance().GetSamplerHeap() };
	//_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	if (!MaterialManager::Instance().SetComputePass(_cmdList, &generateMat))
	{
		return;
	}

	_cmdList->SetComputeRootDescriptorTable(0, _outMip);
	_cmdList->SetComputeRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetComputeRootDescriptorTable(3, _input);
	_cmdList->SetComputeRootDescriptorTable(4, ResourceManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());

	// compute work
	UINT computeKernel = 8;

	for (int i = 0; i < _srcDesc.MipLevels; i += 3)
	{
		// setup start mip level
		int data[] = { i,_srcDesc.MipLevels };
		_cmdList->SetComputeRoot32BitConstants(2, 2, data, 0);

		if ((_srcDesc.Width >> i) < computeKernel || (_srcDesc.Height >> i) < computeKernel)
			_cmdList->Dispatch(1, 1, 1);
		else
			_cmdList->Dispatch((UINT)((_srcDesc.Width >> i) + computeKernel) / computeKernel, (UINT)((_srcDesc.Height >> i) + computeKernel) / computeKernel, 1);
	}
}
