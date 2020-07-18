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
	void InitNativeShadows(int _nativeID, int _numCascade, void** _shadowMapRaw);
	void Release();

	int AddNativeLight(int _instanceID, SqLightData _data);
	void UpdateNativeLight(int _nativeID, SqLightData _data);
	void UpdateNativeShadow(int _nativeID, SqLightData _data);
	void SetShadowFrustum(int _nativeID, XMFLOAT4X4 _view, XMFLOAT4X4 _projCulling, int _cascade);
	void SetViewPortScissorRect(int _nativeID, D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect);
	void UploadPerLightBuffer(int _frameIdx);
	void FillSystemConstant(SystemConstant& _sc);
	void SetPCFKernel(int _kernel);
	void SetAmbientLight(XMFLOAT4 _ag, XMFLOAT4 _as);
	void SetSkybox(void *_skybox, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel, int _skyMeshId);

	Light *GetDirLights();
	int GetNumDirLights();
	Material* GetShadowOpqaue(int _cullMode);
	Material* GetShadowCutout(int _cullMode);
	Material* GetCollectShadow();
	ID3D12DescriptorHeap* GetShadowSampler();
	int GetShadowIndex();

	ID3D12Resource* GetCollectShadowSrc();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCollectShadowRtv();
	D3D12_GPU_VIRTUAL_ADDRESS GetDirLightGPU(int _frameIdx, int _offset);

	Renderer* GetSkyboxRenderer();
	Material* GetSkyboxMat();
	ID3D12DescriptorHeap* GetSkyboxTex();
	ID3D12DescriptorHeap* GetSkyboxSampler();

private:
	int FindLight(vector<Light> _lights, int _instanceID);
	int AddDirLight(int _instanceID, SqLightData _data);
	int AddPointLight(int _instanceID, SqLightData _data);
	int AddSpotLight(int _instanceID, SqLightData _data);
	void AddDirShadow(int _nativeID, int _numCascade, void** _shadowMapRaw);
	void CreateOpaqueShadow(int _instanceID, void *_opaqueShadows);

	// light data
	int maxDirLight;
	int maxPointLight;
	int maxSpotLight;

	vector<Light> dirLights;
	vector<Light> pointLights;
	vector<Light> spotLights;

	unique_ptr<UploadBuffer<SqLightData>> dirLightData[MAX_FRAME_COUNT];
	unique_ptr<UploadBuffer<SqLightData>> pointLightData[MAX_FRAME_COUNT];
	unique_ptr<UploadBuffer<SqLightData>> spotLightData[MAX_FRAME_COUNT];
	unique_ptr<RenderTexture> collectShadow;

	// gi 
	XMFLOAT4 ambientGround;
	XMFLOAT4 ambientSky;
	Texture skyboxTex;
	Sampler skyboxSampler;
	int skyMeshId;
	Material skyboxMat;
	Renderer skyboxRenderer;

	// shadow material
	Material shadowOpaqueMat[CullMode::NumCullMode];
	Material shadowCutoutMat[CullMode::NumCullMode];
	Material collectShadowMat;
	Sampler shadowSampler;
	int collectShadowID;
	int pcfKernel;
};