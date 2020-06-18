#include "Material.h"
#include "GraphicManager.h"

bool Material::CreatePsoFromDesc(D3D12_GRAPHICS_PIPELINE_STATE_DESC _desc)
{
	HRESULT hr = S_OK;

	psoDesc = _desc;
	pso.Reset();
	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateGraphicsPipelineState(&_desc, IID_PPV_ARGS(&pso)), hr);
	validMaterial = SUCCEEDED(hr);
	isDirty = true;

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

void Material::SetCullMode(int _mode)
{
	cullMode = (CullMode)_mode;
}

void Material::SetBlendMode(int _srcBlend, int _dstBlend)
{
	srcBlend = _srcBlend;
	dstBlend = _dstBlend;
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

CullMode Material::GetCullMode()
{
	return cullMode;
}

D3D12_GPU_VIRTUAL_ADDRESS Material::GetMaterialConstantGPU(int _index)
{
	return materialConstant[_index]->Resource()->GetGPUVirtualAddress();
}
