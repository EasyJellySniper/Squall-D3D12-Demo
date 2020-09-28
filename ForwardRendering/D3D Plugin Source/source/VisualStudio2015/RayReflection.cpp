#include "RayReflection.h"
#include "TextureManager.h"
#include "ShaderManager.h"
#include "MaterialManager.h"

void RayReflection::Init(ID3D12Resource* _rayReflection)
{
	rayReflectionSrc = _rayReflection;

	// register uav & srv
	rayReflectionUav = TextureManager::Instance().AddNativeTexture(GetUniqueID(), _rayReflection, TextureInfo(true, false, true, false, false));
	rayReflectionSrv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), _rayReflection, TextureInfo(true, false, false, false, false));

	// compile shader & material
	Shader* rayReflectionShader = ShaderManager::Instance().CompileShader(L"RayTracingReflection.hlsl");
	if (rayReflectionShader != nullptr)
	{
		rayReflectionMat = MaterialManager::Instance().CreateRayTracingMat(rayReflectionShader);
	}
}

void RayReflection::Release()
{
	rayReflectionMat.Release();
}
