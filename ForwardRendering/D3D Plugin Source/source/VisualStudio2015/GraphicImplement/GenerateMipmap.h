#pragma once
#include "../Material.h"
#include <d3d12.h>

class GenerateMipmap
{
public:
	static void Init();
	static void Release();
	static void Generate(ID3D12GraphicsCommandList *_cmdList, D3D12_RESOURCE_DESC _srcDesc, D3D12_GPU_DESCRIPTOR_HANDLE _input, D3D12_GPU_DESCRIPTOR_HANDLE _outMip);

private:
	static Material generateMat;
};