#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;
#include "stdafx.h"
#include "UploadBuffer.h"
#include "FrameResource.h"

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
	void AddMaterialConstant(UINT _byteSize, void* _data);
	void Release();
	void SetRenderQueue(int _queue);

	ID3D12PipelineState* GetPSO();
	ID3D12RootSignature* GetRootSignature();
	int GetRenderQueue();

private:
	MaterialData matData;
	ComPtr<ID3D12PipelineState> pso;
	shared_ptr<UploadBufferAny> materialConstant[MAX_FRAME_COUNT];

	int renderQueue = 2000;
	bool validMaterial = false;
	bool isDirty = true;
};