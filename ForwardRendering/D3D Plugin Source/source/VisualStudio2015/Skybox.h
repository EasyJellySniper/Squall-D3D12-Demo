#pragma once
#include <DirectXMath.h>
using namespace DirectX;
#include "Material.h"
#include "Renderer.h"
#include "Sampler.h"
#include "TextureManager.h"

struct SkyboxData
{
	XMFLOAT4 ambientGround;
	XMFLOAT4 ambientSky;
	float skyIntensity;
};

class Skybox
{
public:
	void Init(ID3D12Resource*_skyboxSrc, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel, int _skyMesh);
	void Release();
	void SetSkyboxData(XMFLOAT4 _ag, XMFLOAT4 _as, float _skyIntensity);

	Renderer* GetRenderer();
	Material* GetMaterial();
	SkyboxData GetSkyboxData();
	D3D12_GPU_DESCRIPTOR_HANDLE GetSkyboxTex();
	D3D12_GPU_DESCRIPTOR_HANDLE GetSkyboxSampler();
	int GetSkyMeshID();

private:
	int skyMeshId;
	Material skyboxMat;
	Renderer skyboxRenderer;
	SkyboxData skyboxData;
	DescriptorHeapData skyboxSrv;
};