#include "LightManager.h"
#include "GraphicManager.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "RayTracingManager.h"

void LightManager::Init(int _numDirLight, int _numPointLight, int _numSpotLight, void* _opaqueShadows, int _opaqueShadowID)
{
	maxLightCount[LightType::Directional] = _numDirLight;
	maxLightCount[LightType::Point] = _numPointLight;
	maxLightCount[LightType::Spot] = _numSpotLight;

	// create srv buffer
	ID3D12Device *device = GraphicManager::Instance().GetDevice();
	for (int i = 0; i < LightType::LightCount; i++)
	{
		for (int j = 0; j < MAX_FRAME_COUNT; j++)
		{
			lightDataGPU[i][j] = make_unique<UploadBuffer<SqLightData>>(device, maxLightCount[i], false);
		}
	}

	D3D_SHADER_MACRO shadowMacro[] = { "_SHADOW_CUTOFF_ON","1",NULL,NULL };
	Shader* shadowPassOpaque = ShaderManager::Instance().CompileShader(L"ShadowPass.hlsl");
	Shader* shadowPassCutoff = ShaderManager::Instance().CompileShader(L"ShadowPass.hlsl", shadowMacro);

	// create shadow material
	for (int i = 0; i < CullMode::NumCullMode; i++)
	{
		if (shadowPassOpaque != nullptr)
		{
			shadowOpaqueMat[i] = MaterialManager::Instance().CreateMaterialDepthOnly(shadowPassOpaque, D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(i + 1));
		}

		if (shadowPassCutoff != nullptr)
		{
			shadowCutoutMat[i] = MaterialManager::Instance().CreateMaterialDepthOnly(shadowPassCutoff, D3D12_FILL_MODE_SOLID, (D3D12_CULL_MODE)(i + 1));
		}
	}

	CreateCollectShadow(_opaqueShadowID, _opaqueShadows);
	CreateRayTracingShadow();
}

void LightManager::InitNativeShadows(int _nativeID, int _numCascade, void** _shadowMapRaw)
{
	AddDirShadow(_nativeID, _numCascade, _shadowMapRaw);
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

	for (int i = 0; i < CullMode::NumCullMode; i++)
	{
		shadowOpaqueMat[i].Release();
		shadowCutoutMat[i].Release();
	}
	collectShadowMat.Release();
	collectRayShadowMat.Release();

	if (collectShadow != nullptr)
	{
		collectShadow->Release();
		collectShadow.reset();
	}

	skyboxMat.Release();
	skyboxRenderer.Release();
	rtShadowMat.Release();
	rayTracingShadow.reset();
}

void LightManager::ClearLight(ID3D12GraphicsCommandList* _cmdList)
{
	auto dirLights = GetDirLights();
	int numDirLight = GetNumDirLights();

	for (int i = 0; i < numDirLight; i++)
	{
		SqLightData* sld = dirLights[i].GetLightData();

		for (int j = 0; j < sld->numCascade; j++)
		{
			_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dirLights[i].GetShadowDsvSrc(j), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
			_cmdList->ClearDepthStencilView(dirLights[i].GetShadowDsv(j), D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
		}
	}

	// clear target
	FLOAT c[] = { 1,1,1,1 };
	_cmdList->ClearRenderTargetView(LightManager::Instance().GetCollectShadowRtv(), c, 0, nullptr);
}

void LightManager::ShadowWork(Camera* _targetCam)
{
	auto dirLights = sqLights[LightType::Directional];
	for (int i = 0; i < GetNumDirLights(); i++)
	{
		if (!dirLights[i].HasShadowMap())
		{
			RayTracingShadow(_targetCam, &dirLights[i]);
			CollectRayShadow(_targetCam);
		}
		else
		{
			// collect shadow
			CollectShadowMap(_targetCam , &dirLights[i], i);
		}
	}
}

void LightManager::RayTracingShadow(Camera* _targetCam, Light* _light)
{
	// use pre gfx list
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
	auto _cmdList = GraphicManager::Instance().GetFrameResource()->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(GraphicManager::Instance().GetFrameResource()->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery())

	// bind root signature
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(), TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	Material *mat = LightManager::Instance().GetRayShadow();
	UINT cbvSrvUavSize = GraphicManager::Instance().GetCbvSrvUavDesciptorSize();

	auto rayShadowSrc = LightManager::Instance().GetRayShadowSrc();
	auto collectShadowSrc = LightManager::Instance().GetCollectShadowSrc();
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// set state
	_cmdList->SetComputeRootSignature(mat->GetRootSignature());
	_cmdList->SetComputeRootDescriptorTable(0, LightManager::Instance().GetRTShadowUav());
	_cmdList->SetComputeRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetComputeRootShaderResourceView(2, RayTracingManager::Instance().GetTopLevelAS()->GetGPUVirtualAddress());
	_cmdList->SetComputeRootShaderResourceView(3, GetLightDataGPU(LightType::Directional, frameIndex, 0));
	_cmdList->SetComputeRootDescriptorTable(4, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(5, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(6, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(7, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootShaderResourceView(8, RayTracingManager::Instance().GetSubMeshInfoGPU());

	// prepare dispatch desc
	auto rtShadowSrc = rayTracingShadow->Resource();
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = mat->GetDispatchRayDesc((UINT)rtShadowSrc->GetDesc().Width, rtShadowSrc->GetDesc().Height);

	// copy hit group identifier
	MaterialManager::Instance().CopyHitGroupIdentifier(mat, frameIndex);

	// setup hit group table
	auto hitGroup = MaterialManager::Instance().GetHitGroupGPU(frameIndex);
	dispatchDesc.HitGroupTable.StartAddress = hitGroup->Resource()->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = hitGroup->Resource()->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = hitGroup->Stride();

	// dispatch rays
	auto dxrCmd = GraphicManager::Instance().GetDxrList();
	dxrCmd->SetPipelineState1(mat->GetDxcPSO());
	dxrCmd->DispatchRays(&dispatchDesc);

	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::RayTracingShadow])
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}

void LightManager::CollectShadowMap(Camera *_targetCam, Light* _light, int _id)
{
	auto currFrameResource = GraphicManager::Instance().GetFrameResource();
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery())

	// collect shadow
	SqLightData* sld = _light->GetLightData();

	D3D12_RESOURCE_BARRIER collect[6];

	collect[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	collect[1] = CD3DX12_RESOURCE_BARRIER::Transition(LightManager::Instance().GetCollectShadowSrc(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
	for (int i = 2; i < sld->numCascade + 2; i++)
	{
		collect[i] = CD3DX12_RESOURCE_BARRIER::Transition(_light->GetShadowDsvSrc(i - 2), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	_cmdList->ResourceBarrier(2 + sld->numCascade, collect);

	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap() , TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// set target
	_cmdList->OMSetRenderTargets(1, &LightManager::Instance().GetCollectShadowRtv(), true, nullptr);
	_cmdList->RSSetViewports(1, &_targetCam->GetViewPort());
	_cmdList->RSSetScissorRects(1, &_targetCam->GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// set material
	_cmdList->SetPipelineState(LightManager::Instance().GetCollectShadow()->GetPSO());
	_cmdList->SetGraphicsRootSignature(LightManager::Instance().GetCollectShadow()->GetRootSignature());
	_cmdList->SetGraphicsRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(currFrameResource->currFrameIndex));
	_cmdList->SetGraphicsRootShaderResourceView(1, GetLightDataGPU(LightType::Directional, currFrameResource->currFrameIndex, _id));
	_cmdList->SetGraphicsRootDescriptorTable(2, _light->GetShadowSrv());
	_cmdList->SetGraphicsRootDescriptorTable(3, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootDescriptorTable(4, LightManager::Instance().GetShadowSampler());

	_cmdList->DrawInstanced(6, 1, 0, 0);
	GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0]);

	// transition to common
	D3D12_RESOURCE_BARRIER finishCollect[6];
	finishCollect[0] = CD3DX12_RESOURCE_BARRIER::Transition(LightManager::Instance().GetCollectShadowSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	finishCollect[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	for (int i = 2; i < sld->numCascade + 2; i++)
	{
		finishCollect[i] = CD3DX12_RESOURCE_BARRIER::Transition(_light->GetShadowDsvSrc(i - 2), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
	}
	_cmdList->ResourceBarrier(2 + sld->numCascade, finishCollect);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::CollectShadowMap])
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}

void LightManager::CollectRayShadow(Camera* _targetCam)
{
	auto currFrameResource = GraphicManager::Instance().GetFrameResource();
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery());

	// transition resource
	D3D12_RESOURCE_BARRIER collect[3];
	collect[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	collect[1] = CD3DX12_RESOURCE_BARRIER::Transition(GetCollectShadowSrc(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
	collect[2] = CD3DX12_RESOURCE_BARRIER::Transition(GetRayShadowSrc(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(3, collect);

	// set heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap() , TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// set target
	_cmdList->OMSetRenderTargets(1, &GetCollectShadowRtv(), true, nullptr);
	
	// set viewport
	auto viewPort = _targetCam->GetViewPort();
	auto scissor = _targetCam->GetScissorRect();
	viewPort.Width = (FLOAT)GetCollectShadowSrc()->GetDesc().Width;
	viewPort.Height = (FLOAT)GetCollectShadowSrc()->GetDesc().Height;
	scissor.right = (LONG)viewPort.Width;
	scissor.bottom = (LONG)viewPort.Height;

	_cmdList->RSSetViewports(1, &viewPort);
	_cmdList->RSSetScissorRects(1, &scissor);
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// set material
	_cmdList->SetPipelineState(collectRayShadowMat.GetPSO());
	_cmdList->SetGraphicsRootSignature(collectRayShadowMat.GetRootSignature());
	_cmdList->SetGraphicsRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(currFrameResource->currFrameIndex));
	_cmdList->SetGraphicsRootDescriptorTable(1, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootDescriptorTable(2, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());

	_cmdList->DrawInstanced(6, 1, 0, 0);
	GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0]);

	// transition back
	collect[0] = CD3DX12_RESOURCE_BARRIER::Transition(GetCollectShadowSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	collect[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	collect[2] = CD3DX12_RESOURCE_BARRIER::Transition(GetRayShadowSrc(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	_cmdList->ResourceBarrier(3, collect);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::CollectShadowMap]);
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}

int LightManager::AddNativeLight(int _instanceID, SqLightData _data)
{
	return AddLight(_instanceID, _data);
}

void LightManager::UpdateNativeLight(int _id, SqLightData _data)
{
	sqLights[_data.type][_id].SetLightData(_data);
}

void LightManager::UpdateNativeShadow(int _nativeID, SqLightData _data)
{
	if (_data.type == LightType::Directional)
	{
		sqLights[_data.type][_nativeID].SetLightData(_data, true);
	}
}

void LightManager::SetShadowFrustum(int _nativeID, XMFLOAT4X4 _view, XMFLOAT4X4 _projCulling, int _cascade)
{
	// currently only dir light needs 
	sqLights[LightType::Directional][_nativeID].SetShadowFrustum(_view, _projCulling, _cascade);
}

void LightManager::SetViewPortScissorRect(int _nativeID, D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect)
{
	// currently only dir light needs 
	sqLights[LightType::Directional][_nativeID].SetViewPortScissorRect(_viewPort, _scissorRect);
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

			// upload cascade
			if (lightList[j].IsShadowDirty(_frameIdx))
			{
				SqLightData* sld = lightList[j].GetLightData();
				for (int k = 0; k < sld->numCascade; k++)
				{
					LightConstant lc;
					lc.sqMatrixShadow = sld->shadowMatrix[k];
					lightList[j].UploadLightConstant(lc, k, _frameIdx);
				}
				lightList[j].SetShadowDirty(false, _frameIdx);
			}
		}
	}

	// upload skybox constant
	ObjectConstant oc;
	oc.sqMatrixWorld = skyboxRenderer.GetWorld();

	if (skyboxRenderer.IsDirty(_frameIdx))
	{
		skyboxRenderer.UpdateObjectConstant(oc, _frameIdx);
	}
}

void LightManager::FillSystemConstant(SystemConstant& _sc)
{
	_sc.numDirLight = (int)sqLights[LightType::Directional].size();
	_sc.numPointLight = (int)sqLights[LightType::Point].size();
	_sc.numSpotLight = (int)sqLights[LightType::Spot].size();
	_sc.collectShadowIndex = collectShadowID;
	_sc.pcfIndex = pcfKernel;
	_sc.ambientGround = ambientGround;
	_sc.ambientSky = ambientSky;
	_sc.skyIntensity = skyIntensity;
	_sc.collectShadowSampler = collectShadowSampler;
	_sc.rayIndex = rtShadowSrv;
}

void LightManager::SetPCFKernel(int _kernel)
{
	pcfKernel = _kernel;
}

void LightManager::SetAmbientLight(XMFLOAT4 _ag, XMFLOAT4 _as, float _skyIntensity)
{
	ambientGround = _ag;
	ambientSky = _as;
	skyIntensity = _skyIntensity;
}

void LightManager::SetSkybox(void* _skybox, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel, int _skyMesh)
{
	auto skyboxSrc = (ID3D12Resource*)_skybox;
	auto desc = skyboxSrc->GetDesc();

	skyboxTexId = TextureManager::Instance().AddNativeTexture(GetUniqueID(), skyboxSrc, TextureInfo(false, true, false, false, false));
	skyboxSampleId = TextureManager::Instance().AddNativeSampler(wrapU, wrapV, wrapW, _anisoLevel, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	skyMeshId = _skyMesh;

	Shader* skyShader = ShaderManager::Instance().CompileShader(L"Skybox.hlsl");
	if (skyShader != nullptr)
	{
		auto rtd = CameraManager::Instance().GetCamera()->GetRenderTargetData();
		skyboxMat = MaterialManager::Instance().CreateMaterialFromShader(skyShader, rtd, D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_FRONT, 1, 0, D3D12_COMPARISON_FUNC_GREATER_EQUAL, false);
		skyboxRenderer.Init(_skyMesh);
	}
}

void LightManager::SetSkyWorld(XMFLOAT4X4 _world)
{
	skyboxRenderer.SetWorld(_world);
}

Light* LightManager::GetDirLights()
{
	return sqLights[LightType::Directional].data();
}

int LightManager::GetNumDirLights()
{
	return (int)sqLights[LightType::Directional].size();
}

Material* LightManager::GetShadowOpqaue(int _cullMode)
{
	return &shadowOpaqueMat[_cullMode];
}

Material* LightManager::GetShadowCutout(int _cullMode)
{
	return &shadowCutoutMat[_cullMode];
}

Material* LightManager::GetCollectShadow()
{
	return &collectShadowMat;
}

Material* LightManager::GetRayShadow()
{
	return &rtShadowMat;
}

D3D12_GPU_DESCRIPTOR_HANDLE LightManager::GetShadowSampler()
{
	auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart(), shadowSamplerID, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	return handle;
}

ID3D12Resource* LightManager::GetRayShadowSrc()
{
	return rayTracingShadow->Resource();
}

int LightManager::GetShadowIndex()
{
	return collectShadowID;
}

ID3D12Resource* LightManager::GetCollectShadowSrc()
{
	return collectShadow->GetRtvSrc(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE LightManager::GetCollectShadowRtv()
{
	return collectShadow->GetRtvCPU(0);
}

D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetLightDataGPU(LightType _type, int _frameIdx, int _offset)
{
	UINT lightSize = sizeof(SqLightData);
	return lightDataGPU[_type][_frameIdx]->Resource()->GetGPUVirtualAddress() + lightSize * _offset;
}

Renderer* LightManager::GetSkyboxRenderer()
{
	return &skyboxRenderer;
}

Material* LightManager::GetSkyboxMat()
{
	return &skyboxMat;
}

D3D12_GPU_DESCRIPTOR_HANDLE LightManager::GetSkyboxTex()
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE tHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart(), skyboxTexId, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	return tHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE LightManager::GetSkyboxSampler()
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE sHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart(), skyboxSampleId, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	return sHandle;
}

int LightManager::GetSkyMeshID()
{
	return skyMeshId;
}

D3D12_GPU_DESCRIPTOR_HANDLE LightManager::GetRTShadowUav()
{
	auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart(), rtShadowUav, GraphicManager::Instance().GetCbvSrvUavDesciptorSize());
	return handle;
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

void LightManager::AddDirShadow(int _nativeID, int _numCascade, void** _shadowMapRaw)
{
	if (_nativeID < 0 || (int)_nativeID >= sqLights[LightType::Directional].size())
	{
		return;
	}

	sqLights[LightType::Directional][_nativeID].InitNativeShadows(_numCascade, _shadowMapRaw);
}

void LightManager::CreateCollectShadow(int _instanceID, void* _opaqueShadows)
{
	if (_opaqueShadows == nullptr)
	{
		return;
	}

	ID3D12Resource* opaqueShadowSrc = (ID3D12Resource*)_opaqueShadows;

	auto desc = opaqueShadowSrc->GetDesc();
	DXGI_FORMAT shadowFormat = GetColorFormatFromTypeless(desc.Format);

	// register to texture manager
	collectShadow = make_unique<Texture>(1, 0, 0);
	collectShadow->InitRTV(opaqueShadowSrc, shadowFormat, false);
	collectShadowID = TextureManager::Instance().AddNativeTexture(_instanceID, opaqueShadowSrc, TextureInfo(true, false, false, false, false));

	// create collect shadow material
	Shader *collectShader = ShaderManager::Instance().CompileShader(L"CollectShadow.hlsl");
	Shader *collectRayShader = ShaderManager::Instance().CompileShader(L"CollectRayShadow.hlsl");

	if (collectShader != nullptr)
	{
		collectShadowMat = MaterialManager::Instance().CreateMaterialPost(collectShader, false, 1, &shadowFormat, DXGI_FORMAT_UNKNOWN);
	}

	if (collectRayShader != nullptr)
	{
		collectRayShadowMat = MaterialManager::Instance().CreateMaterialPost(collectRayShader, false, 1, &shadowFormat, DXGI_FORMAT_UNKNOWN);
	}

	shadowSamplerID = TextureManager::Instance().AddNativeSampler(TextureWrapMode::Border, TextureWrapMode::Border, TextureWrapMode::Border, 8, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);
	collectShadowSampler = TextureManager::Instance().AddNativeSampler(TextureWrapMode::Clamp, TextureWrapMode::Clamp, TextureWrapMode::Clamp, 8, D3D12_FILTER_ANISOTROPIC);

	// create ray tracing shadow uav
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	int w, h;
	GraphicManager::Instance().GetScreenSize(w, h);
	desc.Width = w;
	desc.Height = h;
	rayTracingShadow = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// create ray tracing texture
	rtShadowUav = TextureManager::Instance().AddNativeTexture(GetUniqueID(), rayTracingShadow->Resource(), TextureInfo(false, false, true, false, false));
	rtShadowSrv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), rayTracingShadow->Resource(), TextureInfo());
}

void LightManager::CreateRayTracingShadow()
{
	Shader* rtShadowShader = ShaderManager::Instance().CompileShader(L"RayTracingShadow.hlsl", nullptr);
	if (rtShadowShader != nullptr)
	{
		rtShadowMat = MaterialManager::Instance().CreateRayTracingMat(rtShadowShader);
	}
}
