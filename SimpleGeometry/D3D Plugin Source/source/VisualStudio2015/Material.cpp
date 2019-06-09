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
