#include "ForwardRenderingPath.h"
#include "FrameResource.h"
#include "GraphicManager.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "LightManager.h"
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
		DrawSkyboxPass(_camera, _frameIdx);
		DrawTransparentPass(_camera, _frameIdx);
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
			BindForwardState(targetCam, frameIndex, _threadIndex);

			if (targetCam->GetRenderMode() == RenderMode::WireFrame)
			{
				DrawWireFrame(targetCam, frameIndex, _threadIndex);
			}

			if (targetCam->GetRenderMode() == RenderMode::Depth)
			{
				DrawOpaqueDepth(targetCam, frameIndex, _threadIndex);
			}

			if (targetCam->GetRenderMode() == RenderMode::ForwardPass)
			{
				DrawOpaqueDepth(targetCam, frameIndex, _threadIndex);
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
				DrawShadowPass(currLight, cascadeIndex, frameIndex, _threadIndex);
			}

			GRAPHIC_TIMER_STOP_ADD(GameTimerManager::Instance().gameTime.renderThreadTime[_threadIndex])
		}
		else if (workerType == WorkerType::OpaqueRendering)
		{
			if (targetCam->GetRenderMode() == RenderMode::ForwardPass)
			{
				BindForwardState(targetCam, frameIndex, _threadIndex);
				DrawOpaquePass(targetCam, frameIndex, _threadIndex);
			}

			GRAPHIC_TIMER_STOP_ADD(GameTimerManager::Instance().gameTime.renderThreadTime[_threadIndex])
		}
		else if (workerType == WorkerType::CutoffRendering)
		{
			if (targetCam->GetRenderMode() == RenderMode::ForwardPass)
			{
				BindForwardState(targetCam, frameIndex, _threadIndex);
				DrawCutoutPass(targetCam, frameIndex, _threadIndex);
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
	LogIfFailedWithoutHR(currFrameResource->preGfxAllocator->Reset());
	LogIfFailedWithoutHR(currFrameResource->preGfxList->Reset(currFrameResource->preGfxAllocator, nullptr));

	// reset thread's allocator
	for (int i = 0; i < numWorkerThreads; i++)
	{
		LogIfFailedWithoutHR(currFrameResource->workerGfxAlloc[i]->Reset());
	}

	auto _cmdList = currFrameResource->preGfxList;

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

	// transition resource state to render target
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition((camData->allowMSAA > 1) ? _camera->GetMsaaRtvSrc(0) : _camera->GetRtvSrc(0)
		, D3D12_RESOURCE_STATE_COMMON
		, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// transition to depth write
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition((camData->allowMSAA > 1) ? _camera->GetMsaaDsvSrc() : _camera->GetCameraDepth()
		, D3D12_RESOURCE_STATE_COMMON
		, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// clear render target view and depth view (reversed-z)
	_cmdList->ClearRenderTargetView((camData->allowMSAA > 1) ? _camera->GetMsaaRtv(0) : _camera->GetRtv(0)
		, camData->clearColor, 0, nullptr);

	_cmdList->ClearDepthStencilView((camData->allowMSAA > 1) ? _camera->GetMsaaDsv() : _camera->GetDsv()
		, D3D12_CLEAR_FLAG_DEPTH
		, 0.0f, 0, 0, nullptr);
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
	sc.cameraPos = _camera->GetPosition();
	LightManager::Instance().FillSystemConstant(sc);

	// calc invert view and invert proj and view proj
	XMFLOAT4X4 view = _camera->GetView();
	XMFLOAT4X4 proj = _camera->GetProj();
	XMStoreFloat4x4(&sc.sqMatrixViewProj, XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj));

	sc.sqMatrixInvView = _camera->GetInvView();
	sc.sqMatrixInvProj = _camera->GetInvProj();
	sc.farZ = _camera->GetFarZ();
	sc.nearZ = _camera->GetNearZ();

	GraphicManager::Instance().UploadSystemConstant(sc, frameIndex);
	LightManager::Instance().UploadPerLightBuffer(frameIndex);
}

void ForwardRenderingPath::PrePassWork(Camera* _camera)
{
	workerType = WorkerType::PrePassRendering;
	WakeAndWaitWorker();

	// resolve depth for shadow mapping
	auto _cmdList = currFrameResource->preGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->preGfxAllocator, nullptr));
	ResolveDepthBuffer(_cmdList, _camera);

	// draw transparent depth, useful for shadow mapping or light culling
	DrawTransparentDepth(_cmdList, _camera, frameIndex);

	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::ShadowWork()
{
	SystemConstant sc = GraphicManager::Instance().GetSystemConstantCPU();
	auto dirLights = LightManager::Instance().GetDirLights();
	int numDirLights = LightManager::Instance().GetNumDirLights();

	// rendering
	for (int i = 0; i < numDirLights; i++)
	{
		if (!dirLights[i].HasShadow())
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

void ForwardRenderingPath::BindForwardState(Camera* _camera, int _frameIdx, int _threadIndex)
{
	// get frame resource
	LogIfFailedWithoutHR(currFrameResource->workerGfxList[_threadIndex]->Reset(currFrameResource->workerGfxAlloc[_threadIndex], nullptr));

	// update camera constant
	CameraData* camData = _camera->GetCameraData();
	auto _cmdList = currFrameResource->workerGfxList[_threadIndex];

	// bind
	_cmdList->OMSetRenderTargets(1,
		(camData->allowMSAA > 1) ? &_camera->GetMsaaRtv(0) : &_camera->GetRtv(0),
		true,
		(camData->allowMSAA > 1) ? &_camera->GetMsaaDsv() : &_camera->GetDsv());

	_cmdList->RSSetViewports(1, &_camera->GetViewPort());
	_cmdList->RSSetScissorRects(1, &_camera->GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ForwardRenderingPath::BindDepthObject(ID3D12GraphicsCommandList* _cmdList, Camera* _camera, int _queue, Renderer* _renderer, Material* _mat, Mesh* _mesh, int _frameIdx)
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
	_cmdList->SetGraphicsRootConstantBufferView(0, _renderer->GetObjectConstantGPU(_frameIdx));
	_cmdList->SetGraphicsRootConstantBufferView(1, _mat->GetMaterialConstantGPU(_frameIdx));

	// setup descriptor table gpu
	if (_queue >= RenderQueue::CutoffStart)
	{
		_cmdList->SetGraphicsRootDescriptorTable(2, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
		_cmdList->SetGraphicsRootDescriptorTable(3, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	}
}

void ForwardRenderingPath::BindShadowObject(ID3D12GraphicsCommandList* _cmdList, Light* _light, int _queue, Renderer* _renderer, Material* _mat, Mesh* _mesh, int _frameIdx)
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
	_cmdList->SetGraphicsRootConstantBufferView(0, _renderer->GetObjectConstantGPU(_frameIdx));
	_cmdList->SetGraphicsRootConstantBufferView(1, _light->GetLightConstantGPU(cascadeIndex, _frameIdx));
	_cmdList->SetGraphicsRootConstantBufferView(2, _mat->GetMaterialConstantGPU(_frameIdx));

	// setup descriptor table gpu
	if (_queue >= RenderQueue::CutoffStart)
	{
		_cmdList->SetGraphicsRootDescriptorTable(3, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
		_cmdList->SetGraphicsRootDescriptorTable(4, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	}
}

void ForwardRenderingPath::BindForwardObject(ID3D12GraphicsCommandList *_cmdList, Renderer* _renderer, Material* _mat, Mesh* _mesh, int _frameIdx)
{
	// bind mesh
	_cmdList->IASetVertexBuffers(0, 1, &_mesh->GetVertexBufferView());
	_cmdList->IASetIndexBuffer(&_mesh->GetIndexBufferView());

	// bind pipeline material
	_cmdList->SetPipelineState(_mat->GetPSO());
	_cmdList->SetGraphicsRootSignature(_mat->GetRootSignature());

	// set system/object constant of renderer
	_cmdList->SetGraphicsRootConstantBufferView(0, _renderer->GetObjectConstantGPU(_frameIdx));
	_cmdList->SetGraphicsRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(_frameIdx));
	_cmdList->SetGraphicsRootConstantBufferView(3, _mat->GetMaterialConstantGPU(_frameIdx));
	_cmdList->SetGraphicsRootDescriptorTable(4, TextureManager::Instance().GetTexHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootDescriptorTable(5, TextureManager::Instance().GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart());
	_cmdList->SetGraphicsRootShaderResourceView(6, LightManager::Instance().GetDirLightGPU(_frameIdx, 0));
}

void ForwardRenderingPath::DrawWireFrame(Camera* _camera, int _frameIdx, int _threadIndex)
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
			_cmdList->SetGraphicsRootConstantBufferView(0, r.cache->GetObjectConstantGPU(_frameIdx));

			// draw mesh
			DrawSubmesh(_cmdList, m, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	// close command list and execute
	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::DrawOpaqueDepth(Camera* _camera, int _frameIdx, int _threadIndex)
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
			BindDepthObject(_cmdList, _camera, qr.first, r.cache, objMat, m, _frameIdx);

			// draw mesh
			DrawSubmesh(_cmdList, m, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	// close command list and execute
	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::DrawTransparentDepth(ID3D12GraphicsCommandList* _cmdList, Camera* _camera, int _frameIdx)
{
	// om set target
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetCameraDepth(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
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
			BindDepthObject(_cmdList, _camera, qr.first, r.cache, objMat, m, _frameIdx);

			// draw mesh
			DrawSubmesh(_cmdList, m, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0])
		}
	}

	// finish rendering
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON));
}

void ForwardRenderingPath::DrawShadowPass(Light* _light, int _cascade, int _frameIdx, int _threadIndex)
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
				BindShadowObject(_cmdList, _light, objMat->GetRenderQueue(), r.get(), objMat, m, _frameIdx);

				// draw mesh
				DrawSubmesh(_cmdList, m, j);
				GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
			}
		}
	}

	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::DrawOpaquePass(Camera* _camera, int _frameIdx, int _threadIndex, bool _cutout)
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
			BindForwardObject(_cmdList, r.cache, objMat, m, frameIndex);

			// draw mesh
			DrawSubmesh(_cmdList, m, r.submeshIndex);
			GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[_threadIndex])
		}
	}

	// close command list and execute
	ExecuteCmdList(_cmdList);
}

void ForwardRenderingPath::DrawCutoutPass(Camera* _camera, int _frameIdx, int _threadIndex)
{
	DrawOpaquePass(_camera, _frameIdx, _threadIndex, true);
}

void ForwardRenderingPath::DrawSkyboxPass(Camera* _camera, int _frameIdx)
{
	if (!LightManager::Instance().GetSkyboxRenderer()->GetActive())
	{
		return;
	}

	// reset cmdlist
	auto _cmdList = currFrameResource->preGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->preGfxAllocator, nullptr));

	// bind descriptor
	ID3D12DescriptorHeap* descriptorHeaps[] = { LightManager::Instance().GetSkyboxTex(),LightManager::Instance().GetSkyboxSampler() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// bind target
	CameraData* camData = _camera->GetCameraData();
	_cmdList->OMSetRenderTargets(1, (camData->allowMSAA > 1) ? &_camera->GetMsaaRtv(0) : &_camera->GetRtv(0), true, (camData->allowMSAA > 1) ? &_camera->GetMsaaDsv() : &_camera->GetDsv());
	_cmdList->RSSetViewports(1, &_camera->GetViewPort());
	_cmdList->RSSetScissorRects(1, &_camera->GetScissorRect());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// bind root signature
	auto skyMat = LightManager::Instance().GetSkyboxMat();
	_cmdList->SetPipelineState(skyMat->GetPSO());
	_cmdList->SetGraphicsRootSignature(skyMat->GetRootSignature());
	_cmdList->SetGraphicsRootConstantBufferView(0, LightManager::Instance().GetSkyboxRenderer()->GetObjectConstantGPU(_frameIdx));
	_cmdList->SetGraphicsRootConstantBufferView(1, GraphicManager::Instance().GetSystemConstantGPU(_frameIdx));
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

void ForwardRenderingPath::DrawTransparentPass(Camera* _camera, int _frameIdx)
{
	// get frame resource, reuse BeginFrame's list
	auto _cmdList = currFrameResource->preGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->preGfxAllocator, nullptr));

	// bind descriptor heap, only need to set once, changing descriptor heap isn't good
	ID3D12DescriptorHeap* descriptorHeaps[] = { TextureManager::Instance().GetTexHeap(),TextureManager::Instance().GetSamplerHeap() };
	_cmdList->SetDescriptorHeaps(2, descriptorHeaps);

	// bind
	CameraData* camData = _camera->GetCameraData();
	_cmdList->OMSetRenderTargets(1,
		(camData->allowMSAA > 1) ? &_camera->GetMsaaRtv(0) : &_camera->GetRtv(0),
		true,
		(camData->allowMSAA > 1) ? &_camera->GetMsaaDsv() : &_camera->GetDsv());

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
			BindForwardObject(_cmdList, r.cache, objMat, m, frameIndex);

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
	LogIfFailedWithoutHR(currFrameResource->postGfxAllocator->Reset());
	LogIfFailedWithoutHR(currFrameResource->postGfxList->Reset(currFrameResource->postGfxAllocator, nullptr));

	CameraData* camData = _camera->GetCameraData();
	auto _cmdList = currFrameResource->postGfxList;

	ResolveColorBuffer(_cmdList, _camera);
	CopyDebugDepth(_cmdList, _camera);

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

	// transition resource to resolve or common
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition((camData->allowMSAA > 1) ? _camera->GetMsaaRtvSrc(0) : _camera->GetRtvSrc(0)
		, D3D12_RESOURCE_STATE_RENDER_TARGET
		, (camData->allowMSAA > 1) ? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COMMON));

	// resolve to non-AA target if MSAA enabled
	if (camData->allowMSAA > 1)
	{
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetRtvSrc(0), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RESOLVE_DEST));
		_cmdList->ResolveSubresource(_camera->GetRtvSrc(0), 0, _camera->GetMsaaRtvSrc(0), 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetRtvSrc(0), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COMMON));
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetMsaaRtvSrc(0), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_COMMON));
	}
}

void ForwardRenderingPath::ResolveDepthBuffer(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	CameraData* camData = _camera->GetCameraData();
	bool useMsaa = (camData->allowMSAA > 1);

	// transition to common or srv 
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition((camData->allowMSAA > 1) ? _camera->GetMsaaDsvSrc() : _camera->GetCameraDepth()
		, D3D12_RESOURCE_STATE_DEPTH_WRITE
		, (useMsaa) ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON));

	if (useMsaa)
	{
		// prepare to resolve
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetCameraDepth(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

		// bind resolve depth pipeline
		ID3D12DescriptorHeap* descriptorHeaps[] = { _camera->GetMsaaSrv(0) };
		_cmdList->SetDescriptorHeaps(1, descriptorHeaps);

		_cmdList->OMSetRenderTargets(0, nullptr, true, &_camera->GetDsv());
		_cmdList->RSSetViewports(1, &_camera->GetViewPort());
		_cmdList->RSSetScissorRects(1, &_camera->GetScissorRect());
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		_cmdList->SetPipelineState(_camera->GetPostMaterial()->GetPSO());
		_cmdList->SetGraphicsRootSignature(_camera->GetPostMaterial()->GetRootSignature());
		_cmdList->SetGraphicsRootConstantBufferView(0, _camera->GetPostMaterial()->GetMaterialConstantGPU(frameIndex));
		_cmdList->SetGraphicsRootDescriptorTable(1, _camera->GetMsaaSrv(0)->GetGPUDescriptorHandleForHeapStart());

		_cmdList->DrawInstanced(6, 1, 0, 0);
		GRAPHIC_BATCH_ADD(GameTimerManager::Instance().gameTime.batchCount[0]);

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetMsaaDsvSrc(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON));
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetCameraDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON));
	}
}

void ForwardRenderingPath::CopyDebugDepth(ID3D12GraphicsCommandList* _cmdList, Camera* _camera)
{
	// copy to debug depth
	if (_camera->GetRenderMode() == RenderMode::Depth)
	{
		CameraData* camData = _camera->GetCameraData();
		bool useMsaa = (camData->allowMSAA > 1);

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetCameraDepth(), (useMsaa) ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE));
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetDebugDepth(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
		_cmdList->CopyResource(_camera->GetDebugDepth(), _camera->GetCameraDepth());
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetCameraDepth(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON));
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera->GetDebugDepth(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
	}
}

void ForwardRenderingPath::CollectShadow(Light* _light, int _id)
{
	auto _cmdList = currFrameResource->preGfxList;
	LogIfFailedWithoutHR(_cmdList->Reset(currFrameResource->preGfxAllocator, nullptr));

	// collect shadow
	SqLightData* sld = _light->GetLightData();

	// transition to srv
	for (int i = 0; i < sld->numCascade; i++)
	{
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_light->GetShadowDsvSrc(i), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}

	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(LightManager::Instance().GetCollectShadowSrc(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));
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
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(LightManager::Instance().GetCollectShadowSrc(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(targetCam->GetCameraDepth(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON));
	for (int i = 0; i < sld->numCascade; i++)
	{
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_light->GetShadowDsvSrc(i), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON));
	}

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
