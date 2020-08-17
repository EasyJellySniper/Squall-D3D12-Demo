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
			RendererManager::Instance().FrustumCulling(targetCam, _threadIndex);
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
			RendererManager::Instance().ShadowCulling(currLight, cascadeIndex, _threadIndex);
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

	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery())
	_camera->ClearCamera(_cmdList);
	LightManager::Instance().ClearLight(_cmdList);
	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::BeginFrame])

	// close command list and execute
	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
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
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery())

	workerType = WorkerType::PrePassRendering;
	WakeAndWaitWorker();

	// resolve depth for other application
	_camera->ResolveDepthBuffer(_cmdList, frameIndex);

	// draw transparent depth, useful for other application
	DrawTransparentDepth(_cmdList, _camera);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::PrepassWork])
	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
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
	LightManager::Instance().ShadowWork(targetCam);
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

	if (workerType == WorkerType::OpaqueRendering || workerType == WorkerType::CutoffRendering)
	{
		GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery())
	}

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
			if (!RendererManager::Instance().ValidRenderer(i, renderers))
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
			m->DrawSubMesh(_cmdList, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	// close command list and execute
	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
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
			if (!RendererManager::Instance().ValidRenderer(i, renderers))
			{
				continue;
			}
			auto const r = renderers[i];
			Mesh* m = r.cache->GetMesh();

			// choose pipeline material according to renderqueue
			Material* const objMat = r.cache->GetMaterial(r.submeshIndex);
			BindDepthObject(_cmdList, _camera, qr.first, r.cache, objMat, m);

			// draw mesh
			m->DrawSubMesh(_cmdList, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	// close command list and execute
	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
}

void ForwardRenderingPath::DrawTransparentDepth(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	// copy resolved depth to transparent depth
	D3D12_RESOURCE_STATES before[2] = { D3D12_RESOURCE_STATE_DEPTH_WRITE ,D3D12_RESOURCE_STATE_COMMON };
	D3D12_RESOURCE_STATES after[2] = { D3D12_RESOURCE_STATE_DEPTH_WRITE ,D3D12_RESOURCE_STATE_DEPTH_WRITE };
	GraphicManager::Instance().CopyResourceWithBarrier(_cmdList, _camera->GetCameraDepth(), _camera->GetTransparentDepth(), before, after);

	// om set target
	_cmdList->OMSetRenderTargets(0, nullptr, TRUE, &_camera->GetTransDsv());
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
			if (!RendererManager::Instance().ValidRenderer(i, renderers))
			{
				continue;
			}
			auto const r = renderers[i];
			Mesh* m = r.cache->GetMesh();

			// choose pipeline material according to renderqueue
			Material* const objMat = r.cache->GetMaterial(r.submeshIndex);
			BindDepthObject(_cmdList, _camera, qr.first, r.cache, objMat, m);

			// draw mesh
			m->DrawSubMesh(_cmdList, r.submeshIndex);
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
				m->DrawSubMesh(_cmdList, j);
				GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
			}
		}
	}

	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
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
			if (!RendererManager::Instance().ValidRenderer(i, renderers))
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
			m->DrawSubMesh(_cmdList, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	// close command list and execute
	if (!_cutout)
	{
		GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeOpaque[_threadIndex])
	}
	else
	{
		GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeCutout[_threadIndex])
	}

	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
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
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery())

	// bind descriptor
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(), TextureManager::Instance().GetSamplerHeap() };
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
	_cmdList->SetGraphicsRootDescriptorTable(2, LightManager::Instance().GetSkyboxTex());
	_cmdList->SetGraphicsRootDescriptorTable(3, LightManager::Instance().GetSkyboxSampler());

	// bind mesh and draw
	Mesh* m = MeshManager::Instance().GetMesh(LightManager::Instance().GetSkyMeshID());
	_cmdList->IASetVertexBuffers(0, 1, &m->GetVertexBufferView());
	_cmdList->IASetIndexBuffer(&m->GetIndexBufferView());

	m->DrawSubMesh(_cmdList, 0);
	GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0])

	// execute
	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::SkyboxPass])
	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
}

void ForwardRenderingPath::DrawTransparentPass(Camera* _camera)
{
	// get frame resource, reuse BeginFrame's list
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery())

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
			if (!RendererManager::Instance().ValidRenderer(i, renderers))
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
			m->DrawSubMesh(_cmdList, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0])
		}
	}

	// close command list and execute
	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::TransparentPass])
	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
}

void ForwardRenderingPath::EndFrame(Camera* _camera)
{
	// get frame resource
	LogIfFailedWithoutHR(currFrameResource->mainGfxList->Reset(currFrameResource->mainGfxAllocator, nullptr));

	CameraData* camData = _camera->GetCameraData();
	auto _cmdList = currFrameResource->mainGfxList;
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery())

	// resolve color buffer
	_camera->ResolveColorBuffer(_cmdList);

	// copy rendering result
	CopyRenderResult(_cmdList, _camera);

	// close command list and execute
	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::EndFrame])
	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
}

void ForwardRenderingPath::CopyRenderResult(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	D3D12_RESOURCE_STATES beforeStates[2] = { D3D12_RESOURCE_STATE_RENDER_TARGET ,  D3D12_RESOURCE_STATE_COMMON };
	D3D12_RESOURCE_STATES afterStates[2] = {D3D12_RESOURCE_STATE_COMMON ,D3D12_RESOURCE_STATE_COMMON };

	GraphicManager::Instance().CopyResourceWithBarrier(_cmdList, _camera->GetRtvSrc(), _camera->GetResultSrc(), beforeStates, afterStates);

	// reset render target state
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON));
}

void ForwardRenderingPath::CollectShadow(Light* _light, int _id)
{
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));

	// collect shadow
	SqLightData* sld = _light->GetLightData();

	D3D12_RESOURCE_BARRIER collect[6];

	collect[0] = CD3DX12_RESOURCE_BARRIER::Transition(targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
	_cmdList->RSSetViewports(1, &targetCam->GetViewPort());
	_cmdList->RSSetScissorRects(1, &targetCam->GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// set material
	_cmdList->SetPipelineState(LightManager::Instance().GetCollectShadow()->GetPSO());
	_cmdList->SetGraphicsRootSignature(LightManager::Instance().GetCollectShadow()->GetRootSignature());
	_cmdList->SetGraphicsRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootShaderResourceView(1, LightManager::Instance().GetDirLightGPU(frameIndex, _id));
	_cmdList->SetGraphicsRootDescriptorTable(2, _light->GetShadowSrv());
	_cmdList->SetGraphicsRootDescriptorTable(3, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootDescriptorTable(4, LightManager::Instance().GetShadowSampler());

	_cmdList->DrawInstanced(6, 1, 0, 0);
	GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0]);

	// transition to common
	D3D12_RESOURCE_BARRIER finishCollect[6];
	finishCollect[0] = CD3DX12_RESOURCE_BARRIER::Transition(LightManager::Instance().GetCollectShadowSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
	finishCollect[1] = CD3DX12_RESOURCE_BARRIER::Transition(targetCam->GetTransparentDepth(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	for (int i = 2; i < sld->numCascade + 2; i++)
	{
		finishCollect[i] = CD3DX12_RESOURCE_BARRIER::Transition(_light->GetShadowDsvSrc(i - 2), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
	}
	_cmdList->ResourceBarrier(2 + sld->numCascade, finishCollect);

	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}
