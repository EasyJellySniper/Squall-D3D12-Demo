#include "Material.h"
#include "GraphicManager.h"

bool Material::Initialize(MaterialData _materialData)
{
	HRESULT hr = S_OK;

	matData = _materialData;
	LogIfFailed(GraphicManager::Instance().GetDevice()->CreateGraphicsPipelineState(&matData.graphicPipeline, IID_PPV_ARGS(&pso)), hr);
	validMaterial = SUCCEEDED(hr);

	return validMaterial;
}

void Material::Release()
{
	pso.Reset();
}

ID3D12PipelineState * Material::GetPSO()
{
	return pso.Get();
}

ID3D12RootSignature* Material::GetRootSignature()
{
	return matData.graphicPipeline.pRootSignature;
}
