#pragma once
#include "Light.h"
#include <vector>
#include "FrameResource.h"
#include "UploadBuffer.h"
#include "MaterialManager.h"
#include "Sampler.h"
#include "Renderer.h"
#include "GraphicImplement/Skybox.h"
#include "GraphicImplement/ForwardPlus.h"
#include "GraphicImplement/RayShadow.h"
#include "GraphicImplement/RayReflection.h"
#include "GraphicImplement/RayAmbient.h"
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

	void Init(int _numDirLight, int _numPointLight, int _numSpotLight);
	void InitRayShadow(void* _src);
	void InitRayReflection(void* _src);
	void InitRayAmbient(void* _src, void* _noiseTex);
	void Release();

	void ClearLight(ID3D12GraphicsCommandList* _cmdList);
	void LightWork(Camera *_targetCam);

	int AddNativeLight(int _instanceID, SqLightData _data);
	void UpdateNativeLight(int _nativeID, SqLightData _data);
	void UploadPerLightBuffer(int _frameIdx);
	void FillSystemConstant(SystemConstant& _sc);
	void SetPCFKernel(int _kernel);
	void SetAmbientLight(XMFLOAT4 _ag, XMFLOAT4 _as, float _skyIntensity);
	void SetReflectionData(ReflectionConst _rd);
	void SetSkybox(void *_skybox, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel, int _skyMeshId);
	void SetSkyWorld(XMFLOAT4X4 _world);
	void SetAmbientData(AmbientConstant _ac);

	Light *GetDirLights();
	int GetNumDirLights();
	D3D12_GPU_VIRTUAL_ADDRESS GetLightDataGPU(LightType _type, int _frameIdx, int _offset);

	Skybox* GetSkybox();
	ForwardPlus* GetForwardPlus();

private:
	int FindLight(vector<Light> _lights, int _instanceID);
	int AddLight(int _instanceID, SqLightData _data);

	// light data
	int maxLightCount[LightType::LightCount];
	vector<Light> sqLights[LightType::LightCount];
	unique_ptr<UploadBuffer<SqLightData>> lightDataGPU[LightType::LightCount][MAX_FRAME_COUNT];

	// skybox
	Skybox skybox;

	// ray tracing implement
	RayShadow rayShadow;
	RayReflection rayReflection;
	RayAmbient rayAmbient;

	// forward+ component
	ForwardPlus forwardPlus;
};