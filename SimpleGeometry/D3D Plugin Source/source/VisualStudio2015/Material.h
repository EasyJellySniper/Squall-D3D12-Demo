#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;
#include "stdafx.h"

struct MaterialData
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipeline;
};

class Material
{
public:
	bool Initialize(MaterialData _materialData);
	void Release();

private:
	MaterialData matData;
	ComPtr<ID3D12PipelineState> pso;
	bool validMaterial = false;
};