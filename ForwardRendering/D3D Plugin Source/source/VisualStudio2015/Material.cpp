#include "Material.h"
#include "GraphicManager.h"

bool Material::CreatePsoFromDesc(D3D12_GRAPHICS_PIPELINE_STATE_DESC _desc)
{
	HRESULT hr = S_OK;

	psoDesc = _desc;
	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateGraphicsPipelineState(&_desc, IID_PPV_ARGS(&pso)), hr);
	validMaterial = SUCCEEDED(hr);
	isDirty = true;
	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		materialConstant[i] = nullptr;
	}

	return validMaterial;
}

void Material::AddMaterialConstant(UINT _byteSize, void* _data)
{
	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		materialConstant[i] = make_shared<UploadBufferAny>(GraphicManager::Instance().GetDevice(), 1, true, _byteSize);
		materialConstant[i]->CopyData(0, _data);
	}
}

void Material::Release()
{
	pso.Reset();
	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		materialConstant[i].reset();
	}
}

void Material::SetRenderQueue(int _queue)
{
	renderQueue = _queue;
}

ID3D12PipelineState * Material::GetPSO()
{
	return pso.Get();
}

ID3D12RootSignature* Material::GetRootSignature()
{
	return psoDesc.pRootSignature;
}

int Material::GetRenderQueue()
{
	return renderQueue;
}

D3D12_GPU_VIRTUAL_ADDRESS Material::GetMaterialConstantGPU(int _index)
{
	return materialConstant[_index]->Resource()->GetGPUVirtualAddress();
}
