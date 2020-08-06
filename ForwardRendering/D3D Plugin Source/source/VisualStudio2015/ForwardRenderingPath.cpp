#include "ForwardRenderingPath.h"
#include "FrameResource.h"
#include "GraphicManager.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "LightManager.h"
#include "RayTracingManager.h"
#include "stdafx.h"
#include "d3dx12.h"
#include <algorithm>

void ForwardRenderingPath::CullingWork(Camera* _camera)
{
	GRAPHIC_TIMER_START

	currFrameResource = GraphicManager::Instance().GetFrameResource();
	numWorkerThreads = GraphicManager::Instance().GetThreadCount() - 1;
	targetCam = _camera;
	workerType = WorkerType::Culling;
	GraphicManager::Instance().ResetWorkerThreadFinish();
	GraphicManager::Instance().SetBeginWorkerThreadEvent();
	GraphicManager::Instance().WaitForWorkerThread();

	GRAPHIC_TIMER_STOP_ADD(GameTimerManager::Instance().gameTime.cullingTime)
}

void ForwardRenderingPath::SortingWork(Camera* _camera)
{
	GRAPHIC_TIMER_START

	// sort work
	RendererManager::Instance().SortWork(_camera);

	GRAPHIC_TIMER_STOP_ADD(GameTimerManager::Instance().gameTime.sortingTime)
}

void ForwardRenderingPath::RenderLoop(Camera* _camera, int _frameIdx)
{
	GRAPHIC_TIMER_START

	targetCam = _camera;
	frameIndex = _frameIdx;

	// begin frame
	BeginFrame(_camera);

	// upload work
	UploadWork(_camera);

	// pre pass work
	PrePassWork(_camera);

	// shadow work
	ShadowWork();

	// opaque pass
	workerType = WorkerType::OpaqueRendering;
	WakeAndWaitWorker();

	// cutoff pass
	workerType = WorkerType::CutoffRendering;
	WakeAndWaitWorker();

	// transparent pass, this can only be rendered with 1 thread for correct order
	if (_camera->GetRenderMode() == RenderMode::ForwardPass)
	{
		DrawSkyboxPass(_camera);
		DrawTransparentPass(_camera);
	}

	EndFrame(_camera);

	GRAPHIC_TIMER_STOP_ADD(GameTimerManager::Instance().gameTime.renderTime)
}

void ForwardRenderingPath::WorkerThread(int _threadIndex)
{
	while (true)
	{
		// wait anything notify to work
		GraphicManager::Instance().WaitBeginWorkerThread(_threadIndex);

		GRAPHIC_TIMER_START

		// culling work
		if (workerType == WorkerType::Culling)
		{
			FrustumCulling(_threadIndex);
		}
		else if (workerType == WorkerType::Upload)
		{
			RendererManager::Instance().UploadObjectConstant(targetCam, frameIndex, _threadIndex, numWorkerThreads);
		}
		else if(workerType == WorkerType::PrePassRendering)
		{
			// process render thread
			BindForwardState(targetCam, _threadIndex);

			if (targetCam->GetRenderMode() == RenderMode::WireFrame)
			{
				DrawWireFrame(targetCam, _threadIndex);
			}

			if (targetCam->GetRenderMode() == RenderMode::Depth)
			{
				DrawOpaqueDepth(targetCam, _threadIndex);
			}

			if (targetCam->GetRenderMode() == RenderMode::ForwardPass)
			{
				DrawOpaqueDepth(targetCam, _threadIndex);
			}

			GRAPHIC_TIMER_STOP_ADD(GameTimerManager::Instance().gameTime.renderThreadTime[_threadIndex])
		}
		else if (workerType == WorkerType::ShadowCulling)
		{
			ShadowCulling(currLight, cascadeIndex, _threadIndex);
		}
		else if (workerType == WorkerType::ShadowRendering)
		{
			if (targetCam->GetRenderMode() == RenderMode::ForwardPass)
			{
				BindShadowState(currLight, cascadeIndex, _threadIndex);
				DrawShadowPass(currLight, cascadeIndex, _threadIndex);
			}

			GRAPHIC_TIMER_STOP_ADD(GameTimerManager::Instance().gameTime.renderThreadTime[_threadIndex])
		}
		else if (workerType == WorkerType::OpaqueRendering)
		{
			if (targetCam->GetRenderMode() == RenderMode::ForwardPass)
			{
				BindForwardState(targetCam, _threadIndex);
				DrawOpaquePass(targetCam, _threadIndex);
			}

			GRAPHIC_TIMER_STOP_ADD(GameTimerManager::Instance().gameTime.renderThreadTime[_threadIndex])
		}
		else if (workerType == WorkerType::CutoffRendering)
		{
			if (targetCam->GetRenderMode() == RenderMode::ForwardPass)
			{
				BindForwardState(targetCam, _threadIndex);
				DrawCutoutPass(targetCam, _threadIndex);
			}

			GRAPHIC_TIMER_STOP_ADD(GameTimerManager::Instance().gameTime.renderThreadTime[_threadIndex])
		}

		// set worker finish
		GraphicManager::Instance().SetWorkerThreadFinishEvent(_threadIndex);
	}
}

void ForwardRenderingPath::WakeAndWaitWorker()
{
	GraphicManager::Instance().ResetWorkerThreadFinish();
	GraphicManager::Instance().SetBeginWorkerThreadEvent();
	GraphicManager::Instance().WaitForWorkerThread();
}

void ForwardRenderingPath::FrustumCulling(int _threadIndex)
{
	auto renderers = RendererManager::Instance().GetRenderers();
	int count = (int)renderers.size() / numWorkerThreads + 1;
	int start = _threadIndex * count;

	for (int i = start; i <= start + count; i++)
	{
		if (i >= (int)renderers.size())
		{
			continue;
		}

		bool isVisible = targetCam->FrustumTest(renderers[i]->GetBound());
		renderers[i]->SetVisible(isVisible);
	}
}

void ForwardRenderingPath::ShadowCulling(Light* _light, int _cascade, int _threadIndex)
{
	GRAPHIC_TIMER_START

	auto renderers = RendererManager::Instance().GetRenderers();
	int count = (int)renderers.size() / numWorkerThreads + 1;
	int start = _threadIndex * count;

	for (int i = start; i <= start + count; i++)
	{
		if (i >= (int)renderers.size())
		{
			continue;
		}

		bool shadowVisible = _light->FrustumTest(renderers[i]->GetBound(), _cascade);
		renderers[i]->SetShadowVisible(shadowVisible);
	}

	GRAPHIC_TIMER_STOP_ADD(GameTimerManager::Instance().gameTime.cullingTime)
}

void ForwardRenderingPath::BeginFrame(Camera* _camera)
{
	// get frame resource
	LogIfFailedWithoutHR(currFrameResource->mainGfxAllocator->Reset());
	LogIfFailedWithoutHR(currFrameResource->mainGfxList->Reset(currFrameResource->mainGfxAllocator, nullptr));

	// reset thread's allocator
	for (int i = 0; i < numWorkerThreads; i++)
	{
		LogIfFailedWithoutHR(currFrameResource->workerGfxAlloc[i]->Reset());
	}

	auto _cmdList = currFrameResource->mainGfxList;

#if defined(GRAPHICTIME)
	// timer begin
	_cmdList->EndQuery(GraphicManager::Instance().GetGpuTimeQuery(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
#endif

	ClearCamera(_cmdList, _camera);
	ClearLight(_cmdList);

	// close command list and execute
	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::ClearCamera(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	CameraData* camData = _camera->GetCameraData();
	auto rtvSrc = (camData->allowMSAA > 1) ? _camera->GetMsaaRtvSrc() : _camera->GetRtvSrc();
	auto dsvSrc = (camData->allowMSAA > 1) ? _camera->GetMsaaDsvSrc() : _camera->GetCameraDepth();
	auto hRtv = (camData->allowMSAA > 1) ? _camera->GetMsaaRtv() : _camera->GetRtv();
	auto hDsv = (camData->allowMSAA > 1) ? _camera->GetMsaaDsv() : _camera->GetDsv();

	// transition render buffer
	D3D12_RESOURCE_BARRIER clearBarrier[2];
	clearBarrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(rtvSrc, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
	clearBarrier[1] = CD3DX12_RESOURCE_BARRIER::Transition(dsvSrc, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	_cmdList->ResourceBarrier(2, clearBarrier);

	// clear render target view and depth view (reversed-z)
	_cmdList->ClearRenderTargetView(hRtv, camData->clearColor, 0, nullptr);
	_cmdList->ClearDepthStencilView(hDsv, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
}

void ForwardRenderingPath::ClearLight(ID3D12GraphicsCommandList* _cmdList)
{
	auto dirLights = LightManager::Instance().GetDirLights();
	int numDirLight = LightManager::Instance().GetNumDirLights();

	for (int i = 0; i < numDirLight; i++)
	{
		SqLightData *sld = dirLights[i].GetLightData();

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

void ForwardRenderingPath::UploadWork(Camera *_camera)
{
	workerType = WorkerType::Upload;
	WakeAndWaitWorker();

	SystemConstant sc;
	_camera->FillSystemConstant(sc);
	LightManager::Instance().FillSystemConstant(sc);

	GraphicManager::Instance().UploadSystemConstant(sc, frameIndex);
	LightManager::Instance().UploadPerLightBuffer(frameIndex);
}

void ForwardRenderingPath::PrePassWork(Camera* _camera)
{
	workerType = WorkerType::PrePassRendering;
	WakeAndWaitWorker();

	// resolve depth for shadow mapping
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));

	if (_camera->GetCameraData()->allowMSAA > 1)
	{
		ResolveDepthBuffer(_cmdList, _camera);
	}

	// draw transparent depth, useful for shadow mapping or light culling
	DrawTransparentDepth(_cmdList, _camera);

	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::ShadowWork()
{
	auto dirLights = LightManager::Instance().GetDirLights();
	int numDirLights = LightManager::Instance().GetNumDirLights();

	// rendering
	for (int i = 0; i < numDirLights; i++)
	{
		if (!dirLights[i].HasShadowMap())
		{
			continue;
		}

		// render all cascade
		SqLightData *sld = dirLights[i].GetLightData();

		for (int j = 0; j < sld->numCascade; j++)
		{
			cascadeIndex = j;
			currLight = &dirLights[i];

			// culling renderer
			workerType = WorkerType::ShadowCulling;
			WakeAndWaitWorker();

			// multi thread work
			workerType = WorkerType::ShadowRendering;
			WakeAndWaitWorker();
		}

		// collect shadow
		CollectShadow(&dirLights[i], i);
	}

	// ray tracing shadow
	for (int i = 0; i < numDirLights; i++)
	{
		if (!dirLights[i].HasShadowMap())
		{
			RayTracingShadow(&dirLights[i]);
		}
	}
}

void ForwardRenderingPath::RayTracingShadow(Light* _light)
{
	// use pre gfx list
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));

	// bind root signature
	ID3D12DescriptorHeap* descriptorHeaps[] = { LightManager::Instance().GetRayShadowHeap() };
	_cmdList->SetDescriptorHeaps(1, descriptorHeaps);

	Material *mat = LightManager::Instance().GetRayShadow();
	UINT cbvSrvUavSize = GraphicManager::Instance().GetCbvSrvUavDesciptorSize();

	CD3DX12_GPU_DESCRIPTOR_HANDLE gRayTracing = CD3DX12_GPU_DESCRIPTOR_HANDLE(LightManager::Instance().GetRayShadowHeap()->GetGPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE gDepth = CD3DX12_GPU_DESCRIPTOR_HANDLE(LightManager::Instance().GetRayShadowHeap()->GetGPUDescriptorHandleForHeapStart(), 1, cbvSrvUavSize);
	auto rayShadowSrc = LightManager::Instance().GetRayShadowSrc();
	auto collectShadowSrc = LightManager::Instance().GetCollectShadowSrc();

	// set state
	_cmdList->SetComputeRootSignature(mat->GetRootSignature());
	_cmdList->SetComputeRootDescriptorTable(0, gRayTracing);
	_cmdList->SetComputeRootDescriptorTable(1, gDepth);
	_cmdList->SetComputeRootConstantBufferView(2, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetComputeRootShaderResourceView(3, RayTracingManager::Instance().GetTopLevelAS()->GetGPUVirtualAddress());
	_cmdList->SetComputeRootShaderResourceView(4, LightManager::Instance().GetDirLightGPU(frameIndex, 0));

	// prepare dispatch
	Camera* c = CameraManager::Instance().GetCamera();
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = mat->GetDispatchRayDesc((UINT)c->GetViewPort().Width, (UINT)c->GetViewPort().Height);

	// dispatch rays
	auto dxrCmd = GraphicManager::Instance().GetDxrList();
	dxrCmd->SetPipelineState1(mat->GetDxcPSO());
	dxrCmd->DispatchRays(&dispatchDesc);

	// copy result, collect shadow will be used in pixel shader later
	D3D12_RESOURCE_STATES before[4] = { D3D12_RESOURCE_STATE_UNORDERED_ACCESS ,D3D12_RESOURCE_STATE_COPY_SOURCE ,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ,D3D12_RESOURCE_STATE_COPY_DEST };
	D3D12_RESOURCE_STATES after[4] = { D3D12_RESOURCE_STATE_COPY_SOURCE ,D3D12_RESOURCE_STATE_UNORDERED_ACCESS ,D3D12_RESOURCE_STATE_COPY_DEST ,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
	CopyResourceWithBarrier(_cmdList, rayShadowSrc, collectShadowSrc, before, after);

	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::BindShadowState(Light *_light, int _cascade, int _threadIndex)
{
	LogIfFailedWithoutHR(currFrameResource->workerGfxList[_threadIndex]->Reset(currFrameResource->workerGfxAlloc[_threadIndex], nullptr));
	auto _cmdList = currFrameResource->workerGfxList[_threadIndex];

	// transition to depth write
	_cmdList->OMSetRenderTargets(0, nullptr, TRUE, &_light->GetShadowDsv(_cascade));
	_cmdList->RSSetViewports(1, &_light->GetViewPort());
	_cmdList->RSSetScissorRects(1, &_light->GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ForwardRenderingPath::BindForwardState(Camera* _camera, int _threadIndex)
{
	// get frame resource
	LogIfFailedWithoutHR(currFrameResource->workerGfxList[_threadIndex]->Reset(currFrameResource->workerGfxAlloc[_threadIndex], nullptr));

	// update camera constant
	CameraData* camData = _camera->GetCameraData();
	auto _cmdList = currFrameResource->workerGfxList[_threadIndex];

	// bind
	auto rtv = (camData->allowMSAA > 1) ? &_camera->GetMsaaRtv() : &_camera->GetRtv();
	auto dsv = (camData->allowMSAA > 1) ? &_camera->GetMsaaDsv() : &_camera->GetDsv();

	_cmdList->OMSetRenderTargets(1, rtv, true, dsv);
	_cmdList->RSSetViewports(1, &_camera->GetViewPort());
	_cmdList->RSSetScissorRects(1, &_camera->GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ForwardRenderingPath::BindDepthObject(ID3D12GraphicsCommandList* _cmdList, Camera* _camera, int _queue, Renderer* _renderer, Material* _mat, Mesh* _mesh)
{
	Material* pipeMat = nullptr;
	if (_queue < RenderQueue::CutoffStart)
	{
		pipeMat = _camera->GetPipelineMaterial(MaterialType::DepthPrePassOpaque, _mat->GetCullMode());
	}
	else if (_queue >= RenderQueue::CutoffStart)
	{
		pipeMat = _camera->GetPipelineMaterial(MaterialType::DepthPrePassCutoff, _mat->GetCullMode());
	}

	// bind mesh
	_cmdList->IASetVertexBuffers(0, 1, &_mesh->GetVertexBufferView());
	_cmdList->IASetIndexBuffer(&_mesh->GetIndexBufferView());

	// bind pipeline material
	_cmdList->SetPipelineState(pipeMat->GetPSO());
	_cmdList->SetGraphicsRootSignature(pipeMat->GetRootSignature());

	// set system/object constant of renderer
	_cmdList->SetGraphicsRootConstantBufferView(0, _renderer->GetObjectConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(2, _mat->GetMaterialConstantGPU(frameIndex));

	// setup descriptor table gpu
	if (_queue >= RenderQueue::CutoffStart)
	{
		_cmdList->SetGraphicsRootDescriptorTable(3, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
		_cmdList->SetGraphicsRootDescriptorTable(4, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	}
}

void ForwardRenderingPath::BindShadowObject(ID3D12GraphicsCommandList* _cmdList, Light* _light, int _queue, Renderer* _renderer, Material* _mat, Mesh* _mesh)
{
	// choose shadow material
	Material* pipeMat = nullptr;
	if (_queue < RenderQueue::CutoffStart)
	{
		pipeMat = LightManager::Instance().GetShadowOpqaue(_mat->GetCullMode());
	}
	else if (_queue >= RenderQueue::CutoffStart)
	{
		pipeMat = LightManager::Instance().GetShadowCutout(_mat->GetCullMode());
	}

	// bind mesh
	_cmdList->IASetVertexBuffers(0, 1, &_mesh->GetVertexBufferView());
	_cmdList->IASetIndexBuffer(&_mesh->GetIndexBufferView());

	// bind pipeline material
	_cmdList->SetPipelineState(pipeMat->GetPSO());
	_cmdList->SetGraphicsRootSignature(pipeMat->GetRootSignature());

	// set system/object constant of renderer
	_cmdList->SetGraphicsRootConstantBufferView(0, _renderer->GetObjectConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(1, _light->GetLightConstantGPU(cascadeIndex, frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(2, _mat->GetMaterialConstantGPU(frameIndex));

	// setup descriptor table gpu
	if (_queue >= RenderQueue::CutoffStart)
	{
		_cmdList->SetGraphicsRootDescriptorTable(3, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
		_cmdList->SetGraphicsRootDescriptorTable(4, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	}
}

void ForwardRenderingPath::BindForwardObject(ID3D12GraphicsCommandList *_cmdList, Renderer* _renderer, Material* _mat, Mesh* _mesh)
{
	// bind mesh
	_cmdList->IASetVertexBuffers(0, 1, &_mesh->GetVertexBufferView());
	_cmdList->IASetIndexBuffer(&_mesh->GetIndexBufferView());

	// bind pipeline material
	_cmdList->SetPipelineState(_mat->GetPSO());
	_cmdList->SetGraphicsRootSignature(_mat->GetRootSignature());

	// set system/object constant of renderer
	_cmdList->SetGraphicsRootConstantBufferView(0, _renderer->GetObjectConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(3, _mat->GetMaterialConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootDescriptorTable(4, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootDescriptorTable(5, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootShaderResourceView(6, LightManager::Instance().GetDirLightGPU(frameIndex, 0));
}

void ForwardRenderingPath::DrawWireFrame(Camera* _camera, int _threadIndex)
{
	auto _cmdList = currFrameResource->workerGfxList[_threadIndex];
	
	// set debug wire frame material
	Material *mat = _camera->GetPipelineMaterial(MaterialType::DebugWireFrame, CullMode::Off);
	_cmdList->SetPipelineState(mat->GetPSO());
	_cmdList->SetGraphicsRootSignature(mat->GetRootSignature());

	// loop render-queue
	auto queueRenderers = RendererManager::Instance().GetQueueRenderers();
	for (auto const& qr : queueRenderers)
	{
		auto renderers = qr.second;
		int count = (int)renderers.size() / numWorkerThreads + 1;
		int start = _threadIndex * count;

		for (int i = start; i <= start + count; i++)
		{
			// valid renderer
			if (!ValidRenderer(i, renderers))
			{
				continue;
			}

			// bind mesh
			auto const r = renderers[i];
			Mesh *m = r.cache->GetMesh();
			_cmdList->IASetVertexBuffers(0, 1, &m->GetVertexBufferView());
			_cmdList->IASetIndexBuffer(&m->GetIndexBufferView());

			// set system constant of renderer
			_cmdList->SetGraphicsRootConstantBufferView(0, r.cache->GetObjectConstantGPU(frameIndex));
			_cmdList->SetGraphicsRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));

			// draw mesh
			DrawSubmesh(_cmdList, m, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	// close command list and execute
	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::DrawOpaqueDepth(Camera* _camera, int _threadIndex)
{
	auto _cmdList = currFrameResource->workerGfxList[_threadIndex];

	// bind descriptor heap, only need to set once, changing descriptor heap isn't good
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(),TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// loop render-queue
	auto queueRenderers = RendererManager::Instance().GetQueueRenderers();
	for (auto const& qr : queueRenderers)
	{
		auto renderers = qr.second;
		int count = (int)renderers.size() / numWorkerThreads + 1;
		int start = _threadIndex * count;

		// don't draw transparent
		if (qr.first > RenderQueue::OpaqueLast)
		{
			continue;
		}

		for (int i = start; i <= start + count; i++)
		{
			// valid renderer
			if (!ValidRenderer(i, renderers))
			{
				continue;
			}
			auto const r = renderers[i];
			Mesh* m = r.cache->GetMesh();

			// choose pipeline material according to renderqueue
			Material* const objMat = r.cache->GetMaterial(r.submeshIndex);
			BindDepthObject(_cmdList, _camera, qr.first, r.cache, objMat, m);

			// draw mesh
			DrawSubmesh(_cmdList, m, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	// close command list and execute
	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::DrawTransparentDepth(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	// om set target
	_cmdList->OMSetRenderTargets(0, nullptr, TRUE, &_camera->GetDsv());
	_cmdList->RSSetViewports(1, &_camera->GetViewPort());
	_cmdList->RSSetScissorRects(1, &_camera->GetScissorRect());

	// bind descriptor heap, only need to set once, changing descriptor heap isn't good
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(),TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// loop render-queue
	auto queueRenderers = RendererManager::Instance().GetQueueRenderers();
	for (auto const& qr : queueRenderers)
	{
		auto renderers = qr.second;

		// don't draw opaue
		if (qr.first <= RenderQueue::OpaqueLast)
		{
			continue;
		}

		for (int i = 0; i < renderers.size(); i++)
		{
			// valid renderer
			if (!ValidRenderer(i, renderers))
			{
				continue;
			}
			auto const r = renderers[i];
			Mesh* m = r.cache->GetMesh();

			// choose pipeline material according to renderqueue
			Material* const objMat = r.cache->GetMaterial(r.submeshIndex);
			BindDepthObject(_cmdList, _camera, qr.first, r.cache, objMat, m);

			// draw mesh
			DrawSubmesh(_cmdList, m, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0])
		}
	}
}

void ForwardRenderingPath::DrawShadowPass(Light* _light, int _cascade, int _threadIndex)
{
	auto _cmdList = currFrameResource->workerGfxList[_threadIndex];

	// bind heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(),TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// loop render-queue
	auto renderers = RendererManager::Instance().GetRenderers();
	int count = (int)renderers.size() / numWorkerThreads + 1;
	int start = _threadIndex * count;

	for (int i = start; i <= start + count; i++)
	{
		// valid renderer
		if (i >= renderers.size() || !renderers[i]->GetShadowVisible())
		{
			continue;
		}

		auto const r = renderers[i];
		Mesh* m = r->GetMesh();

		if (m != nullptr)
		{
			for (int j = 0; j < r->GetNumMaterials(); j++)
			{
				// choose pipeline material according to renderqueue
				Material* const objMat = r->GetMaterial(j);
				BindShadowObject(_cmdList, _light, objMat->GetRenderQueue(), r.get(), objMat, m);

				// draw mesh
				DrawSubmesh(_cmdList, m, j);
				GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
			}
		}
	}

	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::DrawOpaquePass(Camera* _camera, int _threadIndex, bool _cutout)
{
	auto _cmdList = currFrameResource->workerGfxList[_threadIndex];

	 // bind descriptor heap, only need to set once, changing descriptor heap isn't good
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(),TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// loop render-queue
	auto queueRenderers = RendererManager::Instance().GetQueueRenderers();
	for (auto const& qr : queueRenderers)
	{
		auto renderers = qr.second;
		int count = (int)renderers.size() / numWorkerThreads + 1;
		int start = _threadIndex * count;

		// onlt draw opaque
		if (qr.first > RenderQueue::OpaqueLast)
		{
			continue;
		}

		if (_cutout)
		{
			// cutout pass check
			if (qr.first < RenderQueue::CutoffStart)
			{
				continue;
			}
		}

		for (int i = start; i <= start + count; i++)
		{
			// valid renderer
			if (!ValidRenderer(i, renderers))
			{
				continue;
			}
			auto const r = renderers[i];
			Mesh* m = r.cache->GetMesh();

			// choose pipeline material according to renderqueue
			Material* const objMat = r.cache->GetMaterial(r.submeshIndex);

			// bind forward object
			BindForwardObject(_cmdList, r.cache, objMat, m);

			// draw mesh
			DrawSubmesh(_cmdList, m, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	// close command list and execute
	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::DrawCutoutPass(Camera* _camera, int _threadIndex)
{
	DrawOpaquePass(_camera, _threadIndex, true);
}

void ForwardRenderingPath::DrawSkyboxPass(Camera* _camera)
{
	if (!LightManager::Instance().GetSkyboxRenderer()->GetActive())
	{
		return;
	}

	// reset cmdlist
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));

	// bind descriptor
	ID3D12DescriptorHeap* descriptorHeaps[] = { LightManager::Instance().GetSkyboxTex(),LightManager::Instance().GetSkyboxSampler() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// bind target
	CameraData* camData = _camera->GetCameraData();
	auto rtv = (camData->allowMSAA > 1) ? &_camera->GetMsaaRtv() : &_camera->GetRtv();
	auto dsv = (camData->allowMSAA > 1) ? &_camera->GetMsaaDsv() : &_camera->GetDsv();

	_cmdList->OMSetRenderTargets(1, rtv, true, dsv);
	_cmdList->RSSetViewports(1, &_camera->GetViewPort());
	_cmdList->RSSetScissorRects(1, &_camera->GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// bind root signature
	auto skyMat = LightManager::Instance().GetSkyboxMat();
	_cmdList->SetPipelineState(skyMat->GetPSO());
	_cmdList->SetGraphicsRootSignature(skyMat->GetRootSignature());
	_cmdList->SetGraphicsRootConstantBufferView(0, LightManager::Instance().GetSkyboxRenderer()->GetObjectConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootDescriptorTable(2, LightManager::Instance().GetSkyboxTex()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootDescriptorTable(3, LightManager::Instance().GetSkyboxSampler()->GetGPUDescriptorHandleForHeapStart());

	// bind mesh and draw
	Mesh* m = MeshManager::Instance().GetMesh(LightManager::Instance().GetSkyMeshID());
	_cmdList->IASetVertexBuffers(0, 1, &m->GetVertexBufferView());
	_cmdList->IASetIndexBuffer(&m->GetIndexBufferView());

	DrawSubmesh(_cmdList, m, 0);
	GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0])

	// execute
	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::DrawTransparentPass(Camera* _camera)
{
	// get frame resource, reuse BeginFrame's list
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));

	// bind descriptor heap, only need to set once, changing descriptor heap isn't good
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(),TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// bind
	CameraData* camData = _camera->GetCameraData();
	auto rtv = (camData->allowMSAA > 1) ? &_camera->GetMsaaRtv() : &_camera->GetRtv();
	auto dsv = (camData->allowMSAA > 1) ? &_camera->GetMsaaDsv() : &_camera->GetDsv();

	_cmdList->OMSetRenderTargets(1, rtv, true,dsv);
	_cmdList->RSSetViewports(1, &_camera->GetViewPort());
	_cmdList->RSSetScissorRects(1, &_camera->GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// loop render-queue
	auto queueRenderers = RendererManager::Instance().GetQueueRenderers();
	for (auto const& qr : queueRenderers)
	{
		auto renderers = qr.second;

		// onlt draw transparent
		if (qr.first <= RenderQueue::OpaqueLast)
		{
			continue;
		}

		for (int i = 0; i < (int)renderers.size(); i++)
		{
			// valid renderer
			if (!ValidRenderer(i, renderers))
			{
				continue;
			}
			auto const r = renderers[i];
			Mesh* m = r.cache->GetMesh();

			// choose pipeline material according to renderqueue
			Material* const objMat = r.cache->GetMaterial(r.submeshIndex);

			// bind forward object
			BindForwardObject(_cmdList, r.cache, objMat, m);

			// draw mesh
			DrawSubmesh(_cmdList, m, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0])
		}
	}

	// close command list and execute
	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::DrawSubmesh(ID3D12GraphicsCommandList *_cmdList, Mesh* _mesh, int _subIndex)
{
	SubMesh sm = _mesh->GetSubMesh(_subIndex);
	_cmdList->DrawIndexedInstanced(sm.IndexCountPerInstance, 1, sm.StartIndexLocation, sm.BaseVertexLocation, 0);
}

void ForwardRenderingPath::EndFrame(Camera* _camera)
{
	// get frame resource
	LogIfFailedWithoutHR(currFrameResource->mainGfxList->Reset(currFrameResource->mainGfxAllocator, nullptr));

	CameraData* camData = _camera->GetCameraData();
	auto _cmdList = currFrameResource->mainGfxList;

	if (camData->allowMSAA > 1)
	{
		ResolveColorBuffer(_cmdList, _camera);
	}

	// copy rendering result
	CopyDebugDepth(_cmdList, _camera);
	CopyRenderResult(_cmdList, _camera);

#if defined(GRAPHICTIME)
	// timer end
	_cmdList->EndQuery(GraphicManager::Instance().GetGpuTimeQuery(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
	_cmdList->ResolveQueryData(GraphicManager::Instance().GetGpuTimeQuery(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, GraphicManager::Instance().GetGpuTimeResult(), 0);
#endif

	// close command list and execute
	ExecuteCmdList(_cmdList);

#if defined(GRAPHICTIME)
	uint64_t* pRes;
	LogIfFailedWithoutHR(GraphicManager::Instance().GetGpuTimeResult()->Map(0, nullptr, reinterpret_cast<void**>(&pRes)));
	uint64_t t1 = pRes[0];
	uint64_t t2 = pRes[1];
	GraphicManager::Instance().GetGpuTimeResult()->Unmap(0, nullptr);

	GameTimerManager::Instance().gameTime.gpuTime = static_cast<double>(t2 - t1) / static_cast<double>(GraphicManager::Instance().GetGpuFreq()) * 1000.0;
#endif
}

void ForwardRenderingPath::ResolveColorBuffer(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	CameraData* camData = _camera->GetCameraData();

	// barrier
	D3D12_RESOURCE_BARRIER resolveColor[2];
	resolveColor[0] = CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetMsaaRtvSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	resolveColor[1] = CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetRtvSrc(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RESOLVE_DEST);

	// barrier
	D3D12_RESOURCE_BARRIER finishResolve[2];
	finishResolve[0] = CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetRtvSrc(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COMMON);
	finishResolve[1] = CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetMsaaRtvSrc(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_COMMON);

	// resolve to non-AA target if MSAA enabled
	_cmdList->ResourceBarrier(2, resolveColor);
	_cmdList->ResolveSubresource(_camera->GetRtvSrc(), 0, _camera->GetMsaaRtvSrc(), 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
	_cmdList->ResourceBarrier(2, finishResolve);
}

void ForwardRenderingPath::ResolveDepthBuffer(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	CameraData* camData = _camera->GetCameraData();

	// prepare to resolve
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetMsaaDsvSrc(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	// bind resolve depth pipeline
	ID3D12DescriptorHeap* descriptorHeaps[] = { _camera->GetMsaaSrv() };
	_cmdList->SetDescriptorHeaps(1, descriptorHeaps);

	_cmdList->OMSetRenderTargets(0, nullptr, true, &_camera->GetDsv());
	_cmdList->RSSetViewports(1, &_camera->GetViewPort());
	_cmdList->RSSetScissorRects(1, &_camera->GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	_cmdList->SetPipelineState(_camera->GetPostMaterial()->GetPSO());
	_cmdList->SetGraphicsRootSignature(_camera->GetPostMaterial()->GetRootSignature());
	_cmdList->SetGraphicsRootConstantBufferView(0, _camera->GetPostMaterial()->GetMaterialConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootDescriptorTable(1, _camera->GetMsaaSrv()->GetGPUDescriptorHandleForHeapStart());

	_cmdList->DrawInstanced(6, 1, 0, 0);
	GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0]);

	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetMsaaDsvSrc(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON));
}

void ForwardRenderingPath::CopyDebugDepth(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	CameraData* camData = _camera->GetCameraData();
	bool useMsaa = (camData->allowMSAA > 1);

	// copy to debug depth
	if (_camera->GetRenderMode() == RenderMode::Depth)
	{
		D3D12_RESOURCE_STATES beforeStates[4] = { D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST };
		D3D12_RESOURCE_STATES afterStates[4] = { D3D12_RESOURCE_STATE_COPY_SOURCE ,D3D12_RESOURCE_STATE_DEPTH_WRITE ,D3D12_RESOURCE_STATE_COPY_DEST ,D3D12_RESOURCE_STATE_COMMON };
		beforeStates[0] = (useMsaa) ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COMMON;

		CopyResourceWithBarrier(_cmdList, _camera->GetCameraDepth(), _camera->GetDebugDepth(), beforeStates, afterStates);
	}
}

void ForwardRenderingPath::CopyRenderResult(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	D3D12_RESOURCE_STATES beforeStates[4] = { D3D12_RESOURCE_STATE_RENDER_TARGET , D3D12_RESOURCE_STATE_COPY_SOURCE ,  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST };
	D3D12_RESOURCE_STATES afterStates[4] = { D3D12_RESOURCE_STATE_COPY_SOURCE ,D3D12_RESOURCE_STATE_COMMON ,D3D12_RESOURCE_STATE_COPY_DEST ,D3D12_RESOURCE_STATE_COMMON };

	CopyResourceWithBarrier(_cmdList, _camera->GetRtvSrc(), _camera->GetResultSrc(), beforeStates, afterStates);

	// reset render target state
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON));
}

void ForwardRenderingPath::CopyResourceWithBarrier(ID3D12GraphicsCommandList* _cmdList, ID3D12Resource* _src, ID3D12Resource* _dst, D3D12_RESOURCE_STATES _beforeCopy[4], D3D12_RESOURCE_STATES _afterCopy[4])
{
	D3D12_RESOURCE_BARRIER copyBefore[2];
	copyBefore[0] = CD3DX12_RESOURCE_BARRIER::Transition(_src, _beforeCopy[0], _beforeCopy[1]);
	copyBefore[1] = CD3DX12_RESOURCE_BARRIER::Transition(_dst, _beforeCopy[2], _beforeCopy[3]);

	D3D12_RESOURCE_BARRIER copyAfter[2];
	copyAfter[0] = CD3DX12_RESOURCE_BARRIER::Transition(_src, _afterCopy[0], _afterCopy[1]);
	copyAfter[1] = CD3DX12_RESOURCE_BARRIER::Transition(_dst, _afterCopy[2], _afterCopy[3]);

	_cmdList->ResourceBarrier(2, copyBefore);
	_cmdList->CopyResource(_dst, _src);
	_cmdList->ResourceBarrier(2, copyAfter);
}

void ForwardRenderingPath::CollectShadow(Light* _light, int _id)
{
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));

	// collect shadow
	SqLightData* sld = _light->GetLightData();

	D3D12_RESOURCE_BARRIER collect[6];

	collect[0] = CD3DX12_RESOURCE_BARRIER::Transition(targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	collect[1] = CD3DX12_RESOURCE_BARRIER::Transition(LightManager::Instance().GetCollectShadowSrc(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
	for (int i = 2; i < sld->numCascade + 2; i++)
	{
		collect[i] = CD3DX12_RESOURCE_BARRIER::Transition(_light->GetShadowDsvSrc(i - 2), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	_cmdList->ResourceBarrier(2 + sld->numCascade, collect);

	ID3D12DescriptorHeap* descriptorHeaps[] = { _light->GetShadowSrv() , LightManager::Instance().GetShadowSampler() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// set target
	_cmdList->OMSetRenderTargets(1, &LightManager::Instance().GetCollectShadowRtv(), true, nullptr);
	_cmdList->RSSetViewports(1, &targetCam->GetViewPort());
	_cmdList->RSSetScissorRects(1, &targetCam->GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// set material
	_cmdList->SetPipelineState(LightManager::Instance().GetCollectShadow()->GetPSO());
	_cmdList->SetGraphicsRootSignature(LightManager::Instance().GetCollectShadow()->GetRootSignature());
	_cmdList->SetGraphicsRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootShaderResourceView(1, LightManager::Instance().GetDirLightGPU(frameIndex, _id));
	_cmdList->SetGraphicsRootDescriptorTable(2, _light->GetShadowSrv()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootDescriptorTable(3, LightManager::Instance().GetShadowSampler()->GetGPUDescriptorHandleForHeapStart());

	_cmdList->DrawInstanced(6, 1, 0, 0);
	GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0]);

	// transition to common
	D3D12_RESOURCE_BARRIER finishCollect[6];
	finishCollect[0] = CD3DX12_RESOURCE_BARRIER::Transition(LightManager::Instance().GetCollectShadowSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	finishCollect[1] = CD3DX12_RESOURCE_BARRIER::Transition(targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	for (int i = 2; i < sld->numCascade + 2; i++)
	{
		finishCollect[i] = CD3DX12_RESOURCE_BARRIER::Transition(_light->GetShadowDsvSrc(i - 2), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
	}
	_cmdList->ResourceBarrier(2 + sld->numCascade, finishCollect);

	ExecuteCmdList(_cmdList);
}

bool ForwardRenderingPath::ValidRenderer(int _index, vector<QueueRenderer> _renderers)
{
	if (_index >= (int)_renderers.size())
	{
		return false;
	}

	auto const r = _renderers[_index];

	if (!r.cache->GetVisible())
	{
		return false;
	}

	Mesh* m = r.cache->GetMesh();
	if (m == nullptr)
	{
		return false;
	}

	return true;
}

void ForwardRenderingPath::ExecuteCmdList(ID3D12GraphicsCommandList* _cmdList)
{
	// close command list and execute
	LogIfFailedWithoutHR(_cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { _cmdList };
	GraphicManager::Instance().ExecuteCommandList(_countof(cmdsLists), cmdsLists);
}
