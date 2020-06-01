#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;
#include "stdafx.h"
#include "UploadBuffer.h"
#include "FrameResource.h"

enum RenderQueue
{
	Opaque = 2000,
	CutoffStart = 2226,
	Cutoff = 2450,
	OpaqueLast = 2500,
	Transparent = 3000,
	TransparentLast = 3500,
};

class Material
{
public:
	bool CreatePsoFromDesc(D3D12_GRAPHICS_PIPELINE_STATE_DESC _desc);
	void AddMaterialConstant(UINT _byteSize, void* _data);
	void Release();
	void SetRenderQueue(int _queue);

	ID3D12PipelineState* GetPSO();
	ID3D12RootSignature* GetRootSignature();
	int GetRenderQueue();
	D3D12_GPU_VIRTUAL_ADDRESS GetMaterialConstantGPU(int _index);

private:
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ComPtr<ID3D12PipelineState> pso;
	shared_ptr<UploadBufferAny> materialConstant[MAX_FRAME_COUNT];

	int renderQueue = 2000;
	bool validMaterial = false;
	bool isDirty = true;
};