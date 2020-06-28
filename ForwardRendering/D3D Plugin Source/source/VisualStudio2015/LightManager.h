#pragma once
#include "Light.h"
#include <vector>
#include "FrameResource.h"
#include "UploadBuffer.h"

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
	void Release();

	int AddNativeLight(int _instanceID, SqLightData _data);
	void UpdateNativeLight(int _nativeID, SqLightData _data);
	void UploadLightBuffer(int _frameIdx);

	ID3D12Resource* GetDirLightResource(int _frameIdx);

private:
	int FindLight(vector<Light> _lights, int _instanceID);
	int AddDirLight(int _instanceID, SqLightData _data);
	int AddPointLight(int _instanceID, SqLightData _data);
	int AddSpotLight(int _instanceID, SqLightData _data);

	int maxDirLight;
	int maxPointLight;
	int maxSpotLight;

	vector<Light> dirLights;
	vector<Light> pointLights;
	vector<Light> spotLights;

	unique_ptr<UploadBuffer<SqLightData>> dirLightData[MAX_FRAME_COUNT];
	unique_ptr<UploadBuffer<SqLightData>> pointLightData[MAX_FRAME_COUNT];
	unique_ptr<UploadBuffer<SqLightData>> spotLightData[MAX_FRAME_COUNT];
};