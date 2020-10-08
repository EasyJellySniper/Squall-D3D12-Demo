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

void GenerateMipmap::Generate(ID3D12GraphicsCommandList* _cmdList)
{
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;

	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(), TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	_cmdList->SetPipelineState(generateMat.GetPSO());
	_cmdList->SetComputeRootSignature(generateMat.GetRootSignatureCompute());
	_cmdList->SetComputeRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetComputeRoot32BitConstant(2, 0, 0);
	_cmdList->SetComputeRootDescriptorTable(4, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());

	// compute work
	//_cmdList->Dispatch(tileCountX, tileCountY, 1);
}
