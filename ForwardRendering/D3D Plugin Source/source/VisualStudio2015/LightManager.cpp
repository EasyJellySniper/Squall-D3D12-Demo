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
	ID3D12Device* device = GraphicManager::Instance().GetDevice();
	for (int i = 0; i < LightType::LightCount; i++)
	{
		for (int j = 0; j < MAX_FRAME_COUNT; j++)
		{
			lightDataGPU[i][j] = make_unique<UploadBuffer<SqLightData>>(device, maxLightCount[i], false);
		}
	}

	CreateCollectShadow(_opaqueShadowID, _opaqueShadows);
	CreateRayTracingShadow();
	CreateForwardPlusResource();
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
	pointLightTiles.reset();
	pointLightTilesTrans.reset();
	forwardPlusTileMat.Release();
}

void LightManager::ClearLight(ID3D12GraphicsCommandList* _cmdList)
{
	// clear target
	FLOAT c[] = { 1,1,1,1 };
	_cmdList->ClearRenderTargetView(LightManager::Instance().GetCollectShadowRtv(), c, 0, nullptr);
}

void LightManager::LightWork(Camera* _targetCam)
{
	TileLightCulling();
	RayTracingShadow(_targetCam);
	CollectRayShadow(_targetCam);
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
	_sc.maxPointLight = maxLightCount[LightType::Point];
	_sc.collectShadowIndex = collectShadowID;
	_sc.pcfIndex = pcfKernel;
	_sc.ambientGround = ambientGround;
	_sc.ambientSky = ambientSky;
	_sc.skyIntensity = skyIntensity;
	_sc.collectShadowSampler = collectShadowSampler;
	_sc.rayIndex = rtShadowSrv;
	_sc.tileCountX = tileCountX;
	_sc.tileCountY = tileCountY;
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
		skyboxMat = MaterialManager::Instance().CreateGraphicMat(skyShader, rtd, D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_FRONT, 1, 0, D3D12_COMPARISON_FUNC_GREATER_EQUAL, false);
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

Material* LightManager::GetRayShadow()
{
	return &rtShadowMat;
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
	return TextureManager::Instance().GetTexHandle(skyboxTexId);
}

D3D12_GPU_DESCRIPTOR_HANDLE LightManager::GetSkyboxSampler()
{
	return TextureManager::Instance().GetSamplerHandle(skyboxSampleId);
}

int LightManager::GetSkyMeshID()
{
	return skyMeshId;
}

D3D12_GPU_DESCRIPTOR_HANDLE LightManager::GetRTShadowUav()
{
	return TextureManager::Instance().GetTexHandle(rtShadowUav);
}

D3D12_GPU_DESCRIPTOR_HANDLE LightManager::GetLightCullingUav()
{
	return TextureManager::Instance().GetTexHandle(pointLightTileUav);
}

D3D12_GPU_DESCRIPTOR_HANDLE LightManager::GetLightCullingSrv()
{
	return TextureManager::Instance().GetTexHandle(pointLightTileSrv);
}

D3D12_GPU_DESCRIPTOR_HANDLE LightManager::GetLightCullingTransUav()
{
	return TextureManager::Instance().GetTexHandle(pointLightTransTileUav);
}

D3D12_GPU_DESCRIPTOR_HANDLE LightManager::GetLightCullingTransSrv()
{
	return TextureManager::Instance().GetTexHandle(pointLightTransTileSrv);
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
	collectShadow = make_unique<Texture>(1, 0);
	collectShadow->InitRTV(opaqueShadowSrc, shadowFormat, false);
	collectShadowID = TextureManager::Instance().AddNativeTexture(_instanceID, opaqueShadowSrc, TextureInfo(true, false, false, false, false));

	// create collect shadow material
	Shader* collectRayShader = ShaderManager::Instance().CompileShader(L"CollectRayShadow.hlsl");
	if (collectRayShader != nullptr)
	{
		collectRayShadowMat = MaterialManager::Instance().CreatePostMat(collectRayShader, false, 1, &shadowFormat, DXGI_FORMAT_UNKNOWN);
	}

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

void LightManager::CreateForwardPlusResource()
{
	int w, h;
	GraphicManager::Instance().GetScreenSize(w, h);

	tileCountX = (int)ceil((float)w / tileSize);
	tileCountY = (int)ceil((float)h / tileSize);

	UINT totalSize = maxLightCount[LightType::Point] * 4 + 4;
	totalSize *= tileCountX * tileCountY;
	pointLightTiles = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), totalSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	pointLightTileUav = TextureManager::Instance().AddNativeTexture(GetUniqueID(), pointLightTiles->Resource(), TextureInfo(false, false, true, false, true, totalSize / 4, 0));
	pointLightTileSrv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), pointLightTiles->Resource(), TextureInfo(false, false, false, false, true, totalSize / 4, 0));

	pointLightTilesTrans = make_unique<DefaultBuffer>(GraphicManager::Instance().GetDevice(), totalSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	pointLightTransTileUav = TextureManager::Instance().AddNativeTexture(GetUniqueID(), pointLightTilesTrans->Resource(), TextureInfo(false, false, true, false, true, totalSize / 4, 0));
	pointLightTransTileSrv = TextureManager::Instance().AddNativeTexture(GetUniqueID(), pointLightTilesTrans->Resource(), TextureInfo(false, false, false, false, true, totalSize / 4, 0));

	auto tileShader = ShaderManager::Instance().CompileShader(L"ForwardPlusTile.hlsl", nullptr);
	if (tileShader != nullptr)
	{
		forwardPlusTileMat = MaterialManager::Instance().CreateComputeMat(tileShader);
	}
}

void LightManager::TileLightCulling()
{
	// use pre gfx list
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
	auto _cmdList = GraphicManager::Instance().GetFrameResource()->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(GraphicManager::Instance().GetFrameResource()->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery());

	// set heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(), TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// set pso & root signature
	_cmdList->SetPipelineState(forwardPlusTileMat.GetPSO());
	_cmdList->SetComputeRootSignature(forwardPlusTileMat.GetRootSignatureCompute());
	_cmdList->SetComputeRootDescriptorTable(0, GetLightCullingUav());
	_cmdList->SetComputeRootDescriptorTable(1, GetLightCullingTransUav());
	_cmdList->SetComputeRootConstantBufferView(2, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetComputeRootShaderResourceView(3, GetLightDataGPU(LightType::Point, frameIndex, 0));
	_cmdList->SetComputeRootDescriptorTable(4, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(5, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());

	// compute work
	_cmdList->Dispatch(tileCountX, tileCountY, 1);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::TileLightCulling]);
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}

void LightManager::RayTracingShadow(Camera* _targetCam)
{
	// use pre gfx list
	auto frameIndex = GraphicManager::Instance().GetFrameResource()->currFrameIndex;
	auto _cmdList = GraphicManager::Instance().GetFrameResource()->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(GraphicManager::Instance().GetFrameResource()->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery());

	// bind root signature
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(), TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	Material* mat = LightManager::Instance().GetRayShadow();
	UINT cbvSrvUavSize = GraphicManager::Instance().GetCbvSrvUavDesciptorSize();

	D3D12_RESOURCE_BARRIER barriers[3];
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetRtvSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetNormalSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(3, barriers);

	// set state
	_cmdList->SetComputeRootSignature(mat->GetRootSignature());
	_cmdList->SetComputeRootDescriptorTable(0, LightManager::Instance().GetRTShadowUav());
	_cmdList->SetComputeRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetComputeRootDescriptorTable(2, GetLightCullingSrv());
	_cmdList->SetComputeRootShaderResourceView(3, RayTracingManager::Instance().GetTopLevelAS()->GetGPUVirtualAddress());
	_cmdList->SetComputeRootShaderResourceView(4, GetLightDataGPU(LightType::Directional, frameIndex, 0));
	_cmdList->SetComputeRootShaderResourceView(5, GetLightDataGPU(LightType::Point, frameIndex, 0));
	_cmdList->SetComputeRootDescriptorTable(6, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(7, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(8, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootDescriptorTable(9, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetComputeRootShaderResourceView(10, RayTracingManager::Instance().GetSubMeshInfoGPU());

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

	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetRtvSrc(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
	barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(_targetCam->GetNormalSrc(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
	_cmdList->ResourceBarrier(3, barriers);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::RayTracingShadow]);
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