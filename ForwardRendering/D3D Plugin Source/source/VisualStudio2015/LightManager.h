#pragma once
#include "Light.h"
#include <vector>
#include "FrameResource.h"
#include "UploadBuffer.h"
#include "MaterialManager.h"
#include "Sampler.h"
#include "Renderer.h"
#include <wrl.h>
using namespace Microsoft::WRL;

class LightManager
{
public:
	LightManager(const LightManager&) = delete;
	LightManager(LightManager&&) = delete;
	LightManager& operator=(const LightManager&) = delete;
	LightManager& operator=(LightManager&&) = delete;

	static LightManager& Instance()
	{
		static LightManager instance;
		return instance;
	}

	LightManager() {}
	~LightManager() {}

	void Init(int _numDirLight, int _numPointLight, int _numSpotLight, void *_opaqueShadows, int _opaqueShadowID);
	void Release();
	void ClearLight(ID3D12GraphicsCommandList* _cmdList);
	void LightWork(Camera *_targetCam);

	int AddNativeLight(int _instanceID, SqLightData _data);
	void UpdateNativeLight(int _nativeID, SqLightData _data);
	void UploadPerLightBuffer(int _frameIdx);
	void FillSystemConstant(SystemConstant& _sc);
	void SetPCFKernel(int _kernel);
	void SetAmbientLight(XMFLOAT4 _ag, XMFLOAT4 _as, float _skyIntensity);
	void SetSkybox(void *_skybox, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel, int _skyMeshId);
	void SetSkyWorld(XMFLOAT4X4 _world);

	Light *GetDirLights();
	int GetNumDirLights();
	Material* GetRayShadow();
	ID3D12Resource* GetRayShadowSrc();
	int GetShadowIndex();

	ID3D12Resource* GetCollectShadowSrc();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCollectShadowRtv();
	D3D12_GPU_VIRTUAL_ADDRESS GetLightDataGPU(LightType _type, int _frameIdx, int _offset);

	Renderer* GetSkyboxRenderer();
	Material* GetSkyboxMat();
	D3D12_GPU_DESCRIPTOR_HANDLE GetSkyboxTex();
	D3D12_GPU_DESCRIPTOR_HANDLE GetSkyboxSampler();
	int GetSkyMeshID();

	D3D12_GPU_DESCRIPTOR_HANDLE GetRTShadowUav();
	D3D12_GPU_DESCRIPTOR_HANDLE GetLightCullingUav();

private:
	int FindLight(vector<Light> _lights, int _instanceID);
	int AddLight(int _instanceID, SqLightData _data);
	void CreateCollectShadow(int _instanceID, void *_opaqueShadows);
	void CreateRayTracingShadow();
	void CreateForwardPlusResource();
	void TileLightCulling();
	void RayTracingShadow(Camera* _targetCam);
	void CollectRayShadow(Camera* _targetCam);

	// light data
	int maxLightCount[LightType::LightCount];
	vector<Light> sqLights[LightType::LightCount];
	unique_ptr<UploadBuffer<SqLightData>> lightDataGPU[LightType::LightCount][MAX_FRAME_COUNT];

	unique_ptr<Texture> collectShadow;
	unique_ptr<DefaultBuffer> rayTracingShadow;

	// gi 
	XMFLOAT4 ambientGround;
	XMFLOAT4 ambientSky;

	int skyboxTexId;
	int skyboxSampleId;
	int skyMeshId;
	Material skyboxMat;
	Renderer skyboxRenderer;
	float skyIntensity;

	// shadow material
	Material collectRayShadowMat;
	int collectShadowID;
	int collectShadowSampler;
	int pcfKernel;

	// ray tracing material
	Material rtShadowMat;
	int rtShadowUav;
	int rtShadowSrv;

	// forward+ component
	unique_ptr<DefaultBuffer> pointLightTiles;
	int tileSize = 32;
	int tileCountX;
	int tileCountY;
	XMFLOAT2 tileBias;
	int pointLightTileUav;
	int pointLightTileSrv;
	Material forwardPlusTileMat;
};