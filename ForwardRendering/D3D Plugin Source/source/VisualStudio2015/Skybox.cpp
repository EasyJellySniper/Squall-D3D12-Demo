#include "Skybox.h"
#include "TextureManager.h"
#include "ShaderManager.h"
#include "CameraManager.h"
#include "MaterialManager.h"

void Skybox::Init(ID3D12Resource* _skyboxSrc, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel, int _skyMesh)
{
	skyboxTexId = TextureManager::Instance().AddNativeTexture(GetUniqueID(), _skyboxSrc, TextureInfo(false, true, false, false, false));
	skyboxSampleId = TextureManager::Instance().AddNativeSampler(wrapU, wrapV, wrapW, _anisoLevel, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	skyMeshId = _skyMesh;

	Shader* skyShader = ShaderManager::Instance().CompileShader(L"Skybox.hlsl");
	if (skyShader != nullptr)
	{
		auto rtd = CameraManager::Instance().GetCamera()->GetRenderTargetData();
		skyboxMat = MaterialManager::Instance().CreateGraphicMat(skyShader, rtd, D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_FRONT, 1, 0, D3D12_COMPARISON_FUNC_GREATER_EQUAL, false);
		skyboxRenderer.Init(_skyMesh, false);
	}
}

void Skybox::Release()
{
	skyboxMat.Release();
	skyboxRenderer.Release();
}

void Skybox::SetSkyboxData(XMFLOAT4 _ag, XMFLOAT4 _as, float _skyIntensity)
{
	skyboxData.ambientGround = _ag;
	skyboxData.ambientSky = _as;
	skyboxData.skyIntensity = _skyIntensity;
}

Renderer* Skybox::GetRenderer()
{
	return &skyboxRenderer;
}

Material* Skybox::GetMaterial()
{
	return &skyboxMat;
}

SkyboxData Skybox::GetSkyboxData()
{
	return skyboxData;
}

D3D12_GPU_DESCRIPTOR_HANDLE Skybox::GetSkyboxTex()
{
	return TextureManager::Instance().GetTexHandle(skyboxTexId);
}

D3D12_GPU_DESCRIPTOR_HANDLE Skybox::GetSkyboxSampler()
{
	return TextureManager::Instance().GetSamplerHandle(skyboxSampleId);
}

int Skybox::GetSkyMeshID()
{
	return skyMeshId;
}
