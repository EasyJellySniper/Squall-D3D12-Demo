#pragma once
#include "../Material.h"
#include <d3d12.h>

class GenerateMipmap
{
public:
	static void Init();
	static void Release();
	static void Generate(ID3D12GraphicsCommandList *_cmdList);

private:
	static Material generateMat;
};