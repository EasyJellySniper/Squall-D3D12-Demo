#pragma once
#include "Light.h"
#include <vector>
#include "FrameResource.h"
#include "UploadBuffer.h"
#include "MaterialManager.h"
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

	void Init(int _numDirLight, int _numPointLight, int _numSpotLight, void *_opaqueShadows);
	void InitNativeShadows(int _nativeID, int _numCascade, void** _shadowMapRaw);
	void Release();

	int AddNativeLight(int _instanceID, SqLightData _data);
	void UpdateNativeLight(int _nativeID, SqLightData _data);
	void UpdateNativeShadow(int _nativeID, SqLightData _data);
	void SetViewPortScissorRect(int _nativeID, D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect);
	void UploadPerLightBuffer(int _frameIdx);
	void FillSystemConstant(SystemConstant& _sc);

	Light *GetDirLights();
	int GetNumDirLights();
	Material* GetShadowOpqaue(int _cullMode);
	Material* GetShadowCutout(int _cullMode);

	ID3D12Resource* GetDirLightResource(int _frameIdx);

private:
	int FindLight(vector<Light> _lights, int _instanceID);
	int AddDirLight(int _instanceID, SqLightData _data);
	int AddPointLight(int _instanceID, SqLightData _data);
	int AddSpotLight(int _instanceID, SqLightData _data);
	void AddDirShadow(int _nativeID, int _numCascade, void** _shadowMapRaw);
	void CreateOpaqueShadow(void *_opaqueShadows);

	int maxDirLight;
	int maxPointLight;
	int maxSpotLight;

	vector<Light> dirLights;
	vector<Light> pointLights;
	vector<Light> spotLights;

	unique_ptr<UploadBuffer<SqLightData>> dirLightData[MAX_FRAME_COUNT];
	unique_ptr<UploadBuffer<SqLightData>> pointLightData[MAX_FRAME_COUNT];
	unique_ptr<UploadBuffer<SqLightData>> spotLightData[MAX_FRAME_COUNT];
	ComPtr<ID3D12DescriptorHeap> opaqueShadowRTV;
	ComPtr<ID3D12DescriptorHeap> opaqueShadowSRV;
	ID3D12Resource* opaqueShadowSrc;

	Material shadowOpaqueMat[CullMode::NumCullMode];
	Material shadowCutoutMat[CullMode::NumCullMode];
};