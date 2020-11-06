#include "FXAA.h"
#include "../ShaderManager.h"
#include "../MaterialManager.h"

Material FXAA::fxaaComputeMat;

void FXAA::Init()
{
	Shader* fxaa = ShaderManager::Instance().CompileShader(L"FXAACompute.hlsl");
	if (fxaa != nullptr)
	{
		fxaaComputeMat = MaterialManager::Instance().CreateComputeMat(fxaa);
	}
}

void FXAA::Release()
{
	fxaaComputeMat.Release();
}
