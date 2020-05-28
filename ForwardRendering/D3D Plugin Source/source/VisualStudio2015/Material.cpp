#include "Material.h"
#include "GraphicManager.h"

bool Material::Initialize(MaterialData _materialData)
{
	HRESULT hr = S_OK;

	matData = _materialData;
	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateGraphicsPipelineState(&matData.graphicPipeline, IID_PPV_ARGS(&pso)), hr);
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

void Material::UpdataMaterialConstant(int _frameIdx, void* _data)
{
	// only upload material constant if it is dirty
	if (isDirty)
	{
		materialConstant[_frameIdx]->CopyData(0, _data);
	}
	isDirty = false;
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
	return matData.graphicPipeline.pRootSignature;
}

int Material::GetRenderQueue()
{
	return renderQueue;
}
