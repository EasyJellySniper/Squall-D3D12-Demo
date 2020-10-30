#include "GaussianBlur.h"
#include "../ShaderManager.h"
#include "../MaterialManager.h"

Material GaussianBlur::blurCompute;

void GaussianBlur::Init()
{
	Shader* shader = ShaderManager::Instance().CompileShader(L"GaussianBlurCompute.hlsl");
	if (shader != nullptr)
	{
		blurCompute = MaterialManager::Instance().CreateComputeMat(shader);
	}
}

void GaussianBlur::Release()
{
	blurCompute.Release();
}
