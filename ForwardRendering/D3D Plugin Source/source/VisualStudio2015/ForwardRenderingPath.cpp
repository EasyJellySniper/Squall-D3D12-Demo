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
	GraphicManager::Instance().WakeAndWaitWorker();

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

	// light work
	LightManager::Instance().LightWork(targetCam);

	// opaque pass
	workerType = WorkerType::OpaqueRendering;
	GraphicManager::Instance().WakeAndWaitWorker();

	// cutoff pass
	workerType = WorkerType::CutoffRendering;
	GraphicManager::Instance().WakeAndWaitWorker();

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
				DrawOpaqueNormalDepth(targetCam, _threadIndex);
			}

			if (targetCam->GetRenderMode() == RenderMode::ForwardPass)
			{
				DrawOpaqueNormalDepth(targetCam, _threadIndex);
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
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}

void ForwardRenderingPath::UploadWork(Camera *_camera)
{
	GRAPHIC_TIMER_START
	workerType = WorkerType::Upload;
	GraphicManager::Instance().WakeAndWaitWorker();

	SystemConstant sc;
	_camera->FillSystemConstant(sc);
	LightManager::Instance().FillSystemConstant(sc);

	GraphicManager::Instance().UploadSystemConstant(sc, frameIndex);
	LightManager::Instance().UploadPerLightBuffer(frameIndex);

	// update ray tracing top level AS
	auto _dxrList = GraphicManager::Instance().GetDxrList();
	LogIfFailedWithoutHR(_dxrList->Reset(currFrameResource->mainGfxAllocator, nullptr));

	GPU_TIMER_START(_dxrList, GraphicManager::Instance().GetGpuTimeQuery());
	RayTracingManager::Instance().UpdateTopAccelerationStructure(_dxrList);
	GPU_TIMER_STOP(_dxrList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::UpdateTopLevelAS]);
	GraphicManager::Instance().ExecuteCommandList(_dxrList);

	GRAPHIC_TIMER_STOP(GameTimerManager::Instance().gameTime.uploadTime)
}

void ForwardRenderingPath::PrePassWork(Camera* _camera)
{
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery())

	workerType = WorkerType::PrePassRendering;
	GraphicManager::Instance().WakeAndWaitWorker();

	// resolve color/depth for other application
	// for now color buffer is normal buffer
	if (_camera->GetCameraData()->allowMSAA > 1)
	{
		GraphicManager::Instance().ResolveColorBuffer(_cmdList, _camera->GetMsaaRtvSrc(), _camera->GetRtvSrc(), DXGI_FORMAT_R16G16B16A16_FLOAT);
	}
	_camera->ResolveDepthBuffer(_cmdList, frameIndex);

	// draw transparent depth, useful for other application
	DrawTransparentNormalDepth(_cmdList, _camera);

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::PrepassWork])
	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
}

void ForwardRenderingPath::BindForwardState(Camera* _camera, int _threadIndex)
{
	// get frame resource
	LogIfFailedWithoutHR(currFrameResource->workerGfxList[_threadIndex]->Reset(currFrameResource->workerGfxAlloc[_threadIndex], nullptr));

	// update camera constant
	CameraData* camData = _camera->GetCameraData();
	auto _cmdList = currFrameResource->workerGfxList[_threadIndex];

	if (workerType == WorkerType::OpaqueRendering || workerType == WorkerType::CutoffRendering || workerType == WorkerType::PrePassRendering)
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
	// bind mesh
	_cmdList->IASetVertexBuffers(0, 1, &_mesh->GetVertexBufferView());
	_cmdList->IASetIndexBuffer(&_mesh->GetIndexBufferView());

	// set system/object constant of renderer
	_cmdList->SetGraphicsRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(1, _renderer->GetObjectConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(2, _mat->GetMaterialConstantGPU(frameIndex));

	// setup descriptor table gpu
	_cmdList->SetGraphicsRootDescriptorTable(3, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootDescriptorTable(4, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
}

void ForwardRenderingPath::BindForwardObject(ID3D12GraphicsCommandList *_cmdList, Renderer* _renderer, Material* _mat, Mesh* _mesh)
{
	// bind mesh
	_cmdList->IASetVertexBuffers(0, 1, &_mesh->GetVertexBufferView());
	_cmdList->IASetIndexBuffer(&_mesh->GetIndexBufferView());

	// set system/object constant of renderer
	_cmdList->SetGraphicsRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));

	// choose tile result for opaque/transparent obj
	if (_mat->GetRenderQueue() <= RenderQueue::OpaqueLast)
		_cmdList->SetGraphicsRootDescriptorTable(1, LightManager::Instance().GetForwardPlus()->GetLightCullingSrv());
	else
		_cmdList->SetGraphicsRootDescriptorTable(1, LightManager::Instance().GetForwardPlus()->GetLightCullingTransSrv());

	_cmdList->SetGraphicsRootConstantBufferView(2, _renderer->GetObjectConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(3, _mat->GetMaterialConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootDescriptorTable(4, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootDescriptorTable(5, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootShaderResourceView(6, LightManager::Instance().GetLightDataGPU(LightType::Directional, frameIndex, 0));
	_cmdList->SetGraphicsRootShaderResourceView(7, LightManager::Instance().GetLightDataGPU(LightType::Point, frameIndex, 0));
}

void ForwardRenderingPath::DrawWireFrame(Camera* _camera, int _threadIndex)
{
	auto _cmdList = currFrameResource->workerGfxList[_threadIndex];
	
	// set debug wire frame material
	Material *mat = _camera->GetPipelineMaterial(MaterialType::DebugWireFrame, CullMode::Off);
	if (!MaterialManager::Instance().SetGraphicPass(_cmdList, mat))
	{
		return;
	}

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
			_cmdList->SetGraphicsRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
			_cmdList->SetGraphicsRootConstantBufferView(1, r.cache->GetObjectConstantGPU(frameIndex));

			// draw mesh
			m->DrawSubMesh(_cmdList, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	// close command list and execute
	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
}

void ForwardRenderingPath::DrawOpaqueNormalDepth(Camera* _camera, int _threadIndex)
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

		Material* lastMat = nullptr;
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

			Material* pipeMat = nullptr;
			if (qr.first < RenderQueue::CutoffStart)
			{
				pipeMat = _camera->GetPipelineMaterial(MaterialType::DepthPrePassOpaque, objMat->GetCullMode());
			}
			else if (qr.first >= RenderQueue::CutoffStart)
			{
				pipeMat = _camera->GetPipelineMaterial(MaterialType::DepthPrePassCutoff, objMat->GetCullMode());
			}

			// bind pipeline material
			if (pipeMat != lastMat)
			{
				if (!MaterialManager::Instance().SetGraphicPass(_cmdList, pipeMat))
				{
					continue;
				}
				lastMat = pipeMat;
			}

			BindDepthObject(_cmdList, _camera, qr.first, r.cache, objMat, m);

			// draw mesh
			m->DrawSubMesh(_cmdList, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeDepth[_threadIndex])
	// close command list and execute
	GraphicManager::Instance().ExecuteCommandList(_cmdList);;
}

void ForwardRenderingPath::DrawTransparentNormalDepth(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	// copy resolved depth to transparent depth
	D3D12_RESOURCE_STATES before[2] = { D3D12_RESOURCE_STATE_DEPTH_WRITE ,D3D12_RESOURCE_STATE_COMMON };
	D3D12_RESOURCE_STATES after[2] = { D3D12_RESOURCE_STATE_DEPTH_WRITE ,D3D12_RESOURCE_STATE_DEPTH_WRITE };
	GraphicManager::Instance().CopyResourceWithBarrier(_cmdList, _camera->GetCameraDepth(), _camera->GetTransparentDepth(), before, after);

	// copy normal buffer from color buffer (reuse color buffer)
	before[0] = D3D12_RESOURCE_STATE_COMMON;
	before[1] = D3D12_RESOURCE_STATE_COMMON;
	after[0] = D3D12_RESOURCE_STATE_COMMON;
	after[1] = D3D12_RESOURCE_STATE_RENDER_TARGET;
	GraphicManager::Instance().CopyResourceWithBarrier(_cmdList, _camera->GetRtvSrc(), _camera->GetNormalSrc(), before, after);

	// om set target
	_cmdList->OMSetRenderTargets(1, &_camera->GetNormalRtv(), TRUE, &_camera->GetTransDsv());
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

		Material* lastMat = nullptr;
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
			Material* pipeMat = _camera->GetPipelineMaterial(MaterialType::DepthPrePassCutoff, objMat->GetCullMode());
			
			// bind pipeline material
			if (lastMat != pipeMat)
			{
				if (!MaterialManager::Instance().SetGraphicPass(_cmdList, pipeMat))
				{
					continue;
				}
				lastMat = pipeMat;
			}

			BindDepthObject(_cmdList, _camera, qr.first, r.cache, objMat, m);

			// draw mesh
			m->DrawSubMesh(_cmdList, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0])
		}
	}

	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetNormalSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
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

		Material *lastMat = nullptr;
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

			// bind pipeline material
			if (lastMat != objMat)
			{
				if (!MaterialManager::Instance().SetGraphicPass(_cmdList, objMat))
				{
					continue;
				}
				lastMat = objMat;
			}

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
	auto skybox = LightManager::Instance().GetSkybox();

	if (!skybox->GetRenderer()->GetActive())
	{
		return;
	}

	// reset cmdlist
	auto _cmdList = currFrameResource->mainGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->mainGfxAllocator, nullptr));
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery());

	auto skyMat = skybox->GetMaterial();
	if (!MaterialManager::Instance().SetGraphicPass(_cmdList, skyMat))
	{
		return;
	}

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

	// bind root object
	_cmdList->SetGraphicsRootConstantBufferView(0, GraphicManager::Instance().GetSystemConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootConstantBufferView(1, skybox->GetRenderer()->GetObjectConstantGPU(frameIndex));
	_cmdList->SetGraphicsRootDescriptorTable(2, skybox->GetSkyboxTex());
	_cmdList->SetGraphicsRootDescriptorTable(3, skybox->GetSkyboxSampler());

	// bind mesh and draw
	Mesh* m = MeshManager::Instance().GetMesh(skybox->GetSkyMeshID());
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

			// bind pipeline material
			if (!MaterialManager::Instance().SetGraphicPass(_cmdList, objMat))
			{
				continue;
			}

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
	GPU_TIMER_START(_cmdList, GraphicManager::Instance().GetGpuTimeQuery());

	// resolve color buffer
	if (camData->allowMSAA > 1)
	{
		GraphicManager::Instance().ResolveColorBuffer(_cmdList, _camera->GetMsaaRtvSrc(), _camera->GetResultSrc(), DXGI_FORMAT_R16G16B16A16_FLOAT);
	}
	else
	{
		D3D12_RESOURCE_STATES before[2] = { D3D12_RESOURCE_STATE_COMMON ,D3D12_RESOURCE_STATE_COMMON };
		GraphicManager::Instance().CopyResourceWithBarrier(_cmdList, _camera->GetRtvSrc(), _camera->GetResultSrc(), before, before);
	}

	// close command list and execute
	GPU_TIMER_STOP(_cmdList, GraphicManager::Instance().GetGpuTimeQuery(), GameTimerManager::Instance().gpuTimeResult[GpuTimeType::EndFrame]);
	GraphicManager::Instance().ExecuteCommandList(_cmdList);
}