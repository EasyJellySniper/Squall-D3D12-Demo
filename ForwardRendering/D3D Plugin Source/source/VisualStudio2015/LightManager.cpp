#include "LightManager.h"
#include "GraphicManager.h"
#include "ShaderManager.h"
#include "ResourceManager.h"
#include "RayTracingManager.h"

void LightManager::Init(int _numDirLight, int _numPointLight, int _numSpotLight)
{
	maxLightCount[LightType::Directional] = _numDirLight;
	maxLightCount[LightType::Point] = _numPointLight;
	maxLightCount[LightType::Spot] = _numSpotLight;

	// create srv buffer
	ID3D12Device* device = GraphicManager::Instance().GetDevice();
	for (int i = 0; i < LightType::LightCount; i++)
	{
		for (int j = 0; j < MAX_FRAME_COUNT; j++)
		{
			lightDataGPU[i][j] = make_unique<UploadBuffer<SqLightData>>(device, maxLightCount[i], false);
		}
	}

	forwardPlus.Init(maxLightCount[LightType::Point]);
}

void LightManager::InitRayShadow(void* _src)
{
	rayShadow.Init(_src);
}

void LightManager::InitRayReflection(void* _src)
{
	rayReflection.Init((ID3D12Resource*)_src);
}

void LightManager::InitRayAmbient(void* _src, void* _noiseTex)
{
	rayAmbient.Init((ID3D12Resource*)_src, (ID3D12Resource*)_noiseTex);
}

void LightManager::Release()
{
	for (int i = 0; i < LightType::LightCount; i++)
	{
		for (int j = 0; j < sqLights[i].size(); j++)
		{
			sqLights[i][j].Release();
		}
		sqLights[i].clear();

		for (int j = 0; j < MAX_FRAME_COUNT; j++)
		{
			lightDataGPU[i][j].reset();
		}
	}

	skybox.Release();
	rayShadow.Relesae();
	rayReflection.Release();
	rayAmbient.Release();
	forwardPlus.Release();
}

void LightManager::ClearLight(ID3D12GraphicsCommandList* _cmdList)
{
	// clear ray tracing work
	rayShadow.Clear(_cmdList);
}

void LightManager::LightWork(Camera* _targetCam)
{
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
	auto dirLightGPU = GetLightDataGPU(LightType::Directional, frameIndex, 0);
	auto pointLightGPU = GetLightDataGPU(LightType::Point, frameIndex, 0);

	forwardPlus.TileLightCulling(pointLightGPU);

	// ray tracing shadow
	rayShadow.RayTracingShadow(_targetCam, GetForwardPlus(), dirLightGPU, pointLightGPU);
	rayShadow.CollectRayShadow(_targetCam);

	// ray tracing reflection
	rayReflection.Trace(_targetCam, GetForwardPlus(), GetSkybox(), dirLightGPU);

	// ray tracing ambient
	rayAmbient.Trace(_targetCam, dirLightGPU);
}

int LightManager::AddNativeLight(int _instanceID, SqLightData _data)
{
	return AddLight(_instanceID, _data);
}

void LightManager::UpdateNativeLight(int _id, SqLightData _data)
{
	sqLights[_data.type][_id].SetLightData(_data);
}

void LightManager::UploadPerLightBuffer(int _frameIdx)
{
	// per light upload
	for (int i = 0; i < LightType::LightCount; i++)
	{
		auto lightList = sqLights[i];
		auto lightData = lightDataGPU[i];

		for (int j = 0; j < (int)lightList.size(); j++)
		{
			// upload light
			if (lightList[j].IsDirty(_frameIdx))
			{
				SqLightData* sld = lightList[j].GetLightData();
				lightData[_frameIdx]->CopyData(j, *sld);
				lightList[j].SetDirty(false, _frameIdx);
			}
		}
	}

	// upload skybox constant
	ObjectConstant oc;
	oc.sqMatrixWorld = skybox.GetRenderer()->GetWorld();

	if (skybox.GetRenderer()->IsDirty(_frameIdx))
	{
		skybox.GetRenderer()->UpdateObjectConstant(oc, _frameIdx);
	}
}

void LightManager::FillSystemConstant(SystemConstant& _sc)
{
	auto skyData = skybox.GetSkyboxData();
	auto rayShadowData = rayShadow.GetRayShadowData();

	_sc.numDirLight = (int)sqLights[LightType::Directional].size();
	_sc.numPointLight = (int)sqLights[LightType::Point].size();
	_sc.numSpotLight = (int)sqLights[LightType::Spot].size();
	_sc.maxPointLight = maxLightCount[LightType::Point];
	_sc.collectShadowIndex = rayShadowData.collectShadowID;
	_sc.collectTransShadowIndex = rayShadowData.collectTransShadowID;
	_sc.ambientGround = skyData.ambientGround;
	_sc.ambientSky = skyData.ambientSky;
	_sc.skyIntensity = skyData.skyIntensity;
	_sc.cameraPos.w = reflectionDistance;
	_sc.reflectionRTIndex = rayReflection.GetRayReflectionHeap().srv;
	_sc.transReflectionRTIndex = rayReflection.GetTransRayReflectionHeap().srv;
	_sc.ambientRTIndex = rayAmbient.GetAmbientSrv();
	_sc.sqSkyboxWorld = skyData.worldMatrix;

	forwardPlus.GetTileCount(_sc.tileCountX, _sc.tileCountY);

	// copy hit group data (choose one material, all hitgroup is shared)
	// copy hit group identifier
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
}

void LightManager::SetPCFKernel(int _kernel)
{
	rayShadow.SetPCFKernel(_kernel);
}

void LightManager::SetAmbientLight(XMFLOAT4 _ag, XMFLOAT4 _as, float _skyIntensity)
{
	skybox.SetSkyboxData(_ag, _as, _skyIntensity);
}

void LightManager::SetRayDistance(float _reflectionDist)
{
	reflectionDistance = _reflectionDist;
}

void LightManager::SetSkybox(void* _skybox, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel, int _skyMesh)
{
	auto skyboxSrc = (ID3D12Resource*)_skybox;
	auto desc = skyboxSrc->GetDesc();
	skybox.Init(skyboxSrc, wrapU, wrapV, wrapW, _anisoLevel, _skyMesh);
}

void LightManager::SetSkyWorld(XMFLOAT4X4 _world)
{
	skybox.GetRenderer()->SetWorld(_world);
}

void LightManager::SetAmbientData(AmbientConstant _ac)
{
	rayAmbient.UpdataAmbientData(_ac);
}

Light* LightManager::GetDirLights()
{
	return sqLights[LightType::Directional].data();
}

int LightManager::GetNumDirLights()
{
	return (int)sqLights[LightType::Directional].size();
}

D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetLightDataGPU(LightType _type, int _frameIdx, int _offset)
{
	UINT lightSize = sizeof(SqLightData);
	return lightDataGPU[_type][_frameIdx]->Resource()->GetGPUVirtualAddress() + lightSize * _offset;
}

Skybox* LightManager::GetSkybox()
{
	return &skybox;
}

ForwardPlus* LightManager::GetForwardPlus()
{
	return &forwardPlus;
}

int LightManager::FindLight(vector<Light> _lights, int _instanceID)
{
	for (int i = 0; i < (int)_lights.size(); i++)
	{
		if (_lights[i].GetInstanceID() == _instanceID)
		{
			return i;
		}
	}

	return -1;
}

int LightManager::AddLight(int _instanceID, SqLightData _data)
{
	// max light reached
	if ((int)sqLights[_data.type].size() == maxLightCount[_data.type])
	{
		return -1;
	}

	int idx = FindLight(sqLights[_data.type], _instanceID);

	if (idx == -1)
	{
		Light newLight;
		newLight.Init(_instanceID, _data);
		sqLights[_data.type].push_back(newLight);
		idx = (int)sqLights[_data.type].size() - 1;

		// copy to buffer
		for (int i = 0; i < MAX_FRAME_COUNT; i++)
		{
			lightDataGPU[_data.type][i]->CopyData(idx, _data);
		}
	}

	return idx;
}