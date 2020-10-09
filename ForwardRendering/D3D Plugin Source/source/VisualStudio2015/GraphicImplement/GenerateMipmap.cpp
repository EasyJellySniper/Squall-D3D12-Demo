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
	//ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(), TextureManager::Instance().GetSamplerHeap() };
	//_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	_cmdList->SetPipelineState(generateMat.GetPSO());
	_cmdList->SetComputeRootSignature(generateMat.GetRootSignatureCompute());
	_cmdList->SetComputeRootDescriptorTable(0, _outMip);
	_cmdList->SetComputeRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetComputeRoot32BitConstant(2, _srcDesc.MipLevels, 0);
	_cmdList->SetComputeRootDescriptorTable(3, _input);
	_cmdList->SetComputeRootDescriptorTable(4, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());

	// compute work
	UINT computeKernel = 8;

	if (_srcDesc.Width < computeKernel || _srcDesc.Height < computeKernel)
		_cmdList->Dispatch(1, 1, 1);
	else
		_cmdList->Dispatch((UINT)_srcDesc.Width / computeKernel, (UINT)_srcDesc.Height / computeKernel, 1);
}
