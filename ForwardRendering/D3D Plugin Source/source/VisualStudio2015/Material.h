#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;
#include "stdafx.h"

struct MaterialData
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipeline;
};

enum RenderQueue
{
	Opaque = 2000,
	Cutoff = 2450,
	OpaqueLast = 2500,
	Transparent = 3000,
	TransparentLast = 3500,
};

class Material
{
public:
	bool Initialize(MaterialData _materialData);
	void Release();
	ID3D12PipelineState* GetPSO();
	ID3D12RootSignature* GetRootSignature();

private:
	MaterialData matData;
	ComPtr<ID3D12PipelineState> pso;
	bool validMaterial = false;
};