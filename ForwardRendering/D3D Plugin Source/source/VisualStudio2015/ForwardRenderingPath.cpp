#include "ForwardRenderingPath.h"
#include "FrameResource.h"
#include "GraphicManager.h"
#include "ShaderManager.h"
#include "stdafx.h"
#include "d3dx12.h"
#include <algorithm>

void ForwardRenderingPath::CullingWork(Camera _camera)
{
#if defined(GRAPHICTIME)
	TIMER_INIT
	TIMER_START
#endif

	targetCam = _camera;
	workerType = WorkerType::Culling;
	GraphicManager::Instance().ResetWorkerThreadFinish();
	GraphicManager::Instance().SetBeginWorkerThreadEvent();
	GraphicManager::Instance().WaitForWorkerThread();

#if defined(GRAPHICTIME)
	TIMER_STOP
	GameTimerManager::Instance().gameTime.cullingTime += elapsedTime;
#endif
}

void ForwardRenderingPath::SortingWork(Camera _camera)
{
#if defined(GRAPHICTIME)
	TIMER_INIT
	TIMER_START
#endif

	// group to queue renderer
	RendererManager::Instance().ClearQueueRenderer();
	auto renderers = RendererManager::Instance().GetRenderers();
	for (int i = 0; i < (int)renderers.size(); i++)
	{
		if (renderers[i]->GetVisible())
		{
			RendererManager::Instance().AddToQueueRenderer(renderers[i].get(), _camera);
		}
	}

	// sort each queue renderers
	auto queueRenderers = RendererManager::Instance().GetQueueRenderers();
	for (auto& qr : queueRenderers)
	{
		if (qr.first <= RenderQueue::OpaqueLast)
		{
			// sort front-to-back
			sort(qr.second.begin(), qr.second.end(), FrontToBackRender);
		}
		else
		{
			// sort back-to-front
			sort(qr.second.begin(), qr.second.end(), BackToFrontRender);
		}
	}

#if defined(GRAPHICTIME)
	TIMER_STOP
	GameTimerManager::Instance().gameTime.sortingTime += elapsedTime;
#endif
}

void ForwardRenderingPath::RenderLoop(Camera _camera, int _frameIdx)
{
#if defined(GRAPHICTIME)
	TIMER_INIT
	TIMER_START
#endif

	BeginFrame(_camera);
	targetCam = _camera;
	workerType = WorkerType::Rendering;
	frameIndex = _frameIdx;
	GraphicManager::Instance().ResetWorkerThreadFinish();
	GraphicManager::Instance().SetBeginWorkerThreadEvent();
	GraphicManager::Instance().WaitForWorkerThread();
	EndFrame(_camera);

#if defined(GRAPHICTIME)
	TIMER_STOP
	GameTimerManager::Instance().gameTime.renderTime += elapsedTime;
#endif
}

void ForwardRenderingPath::WorkerThread(int _threadIndex)
{
	while (true)
	{
		// wait anything notify to work
		GraphicManager::Instance().WaitBeginWorkerThread(_threadIndex);

		// culling work
		if (workerType == WorkerType::Culling)
		{
			FrustumCulling(_threadIndex);
		}
		else if(workerType == WorkerType::Rendering)
		{
			// process render thread
#if defined(GRAPHICTIME)
			TIMER_INIT
			TIMER_START
#endif

			UploadConstant(targetCam, frameIndex, _threadIndex);
			BindState(targetCam, frameIndex, _threadIndex);
			DrawScene(targetCam, frameIndex, _threadIndex);

#if defined(GRAPHICTIME)
			TIMER_STOP
			GameTimerManager::Instance().gameTime.renderThreadTime[_threadIndex] = elapsedTime;
#endif
		}

		// set worker finish
		GraphicManager::Instance().SetWorkerThreadFinishEvent(_threadIndex);
	}
}

void ForwardRenderingPath::FrustumCulling(int _threadIndex)
{
	auto renderers = RendererManager::Instance().GetRenderers();
	int threads = GraphicManager::Instance().GetThreadCount() - 1;

	int count = (int)renderers.size() / threads + 1;
	int start = _threadIndex * count;

	for (int i = start; i <= start + count; i++)
	{
		if (i >= (int)renderers.size())
		{
			continue;
		}

		bool isVisible = targetCam.FrustumTest(renderers[i]->GetBound());
		renderers[i]->SetVisible(isVisible);
	}
}

void ForwardRenderingPath::BeginFrame(Camera _camera)
{
	// get frame resource
	FrameResource fr = GraphicManager::Instance().GetFrameResource();
	LogIfFailedWithoutHR(fr.preGfxAllocator->Reset());
	LogIfFailedWithoutHR(fr.preGfxList->Reset(fr.preGfxAllocator, nullptr));

	CameraData camData = _camera.GetCameraData();
	auto _cmdList = fr.preGfxList;

#if defined(GRAPHICTIME)
	// timer begin
	_cmdList->EndQuery(GraphicManager::Instance().GetGpuTimeQuery(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
#endif

	// transition resource state to render target
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition((camData.allowMSAA > 1) ? _camera.GetMsaaRtvSrc(0) : _camera.GetRtvSrc(0)
		, D3D12_RESOURCE_STATE_COMMON
		, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// transition to depth write
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition((camData.allowMSAA > 1) ? _camera.GetMsaaDsvSrc() : _camera.GetDsvSrc()
		, D3D12_RESOURCE_STATE_COMMON
		, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// clear render target view and depth view (reversed-z)
	_cmdList->ClearRenderTargetView((camData.allowMSAA > 1) ? _camera.GetMsaaRtv()->GetCPUDescriptorHandleForHeapStart() : _camera.GetRtv()->GetCPUDescriptorHandleForHeapStart()
		, camData.clearColor, 0, nullptr);

	_cmdList->ClearDepthStencilView((camData.allowMSAA > 1) ? _camera.GetMsaaDsv()->GetCPUDescriptorHandleForHeapStart() : _camera.GetDsv()->GetCPUDescriptorHandleForHeapStart()
		, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL
		, 0.0f, 0, 0, nullptr);

	// close command list and execute
	LogIfFailedWithoutHR(_cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { _cmdList };
	GraphicManager::Instance().ExecuteCommandList(_countof(cmdsLists), cmdsLists);
}

void ForwardRenderingPath::UploadConstant(Camera _camera, int _frameIdx, int _threadIndex)
{
	auto renderers = RendererManager::Instance().GetRenderers();

	// split thread group
	int threads = GraphicManager::Instance().GetThreadCount() - 1;
	int count = (int)renderers.size() / threads + 1;
	int start = _threadIndex * count;

	// update renderer constant
	for (int i = start; i <= start + count; i++)
	{
		if (i >= (int)renderers.size())
		{
			continue;
		}

		auto r = renderers[i];
		if (!r->GetVisible())
		{
			continue;
		}

		Mesh* m = r->GetMesh();
		if (m == nullptr)
		{
			continue;
		}

		XMFLOAT4X4 world = r->GetWorld();
		XMFLOAT4X4 view = _camera.GetViewMatrix();
		XMFLOAT4X4 proj = _camera.GetProjMatrix();

		SystemConstant sc;
		XMStoreFloat4x4(&sc.sqMatrixMvp, XMLoadFloat4x4(&world) * XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj));
		r->UpdateSystemConstant(sc, _frameIdx);
	}
}

void ForwardRenderingPath::BindState(Camera _camera, int _frameIdx, int _threadIndex)
{
	// get frame resource
	FrameResource fr = GraphicManager::Instance().GetFrameResource();
	LogIfFailedWithoutHR(fr.workerGfxAlloc[_threadIndex]->Reset());
	LogIfFailedWithoutHR(fr.workerGfxList[_threadIndex]->Reset(fr.workerGfxAlloc[_threadIndex], nullptr));

	// update camera constant
	CameraData camData = _camera.GetCameraData();
	auto _cmdList = fr.workerGfxList[_threadIndex];

	// bind
	_cmdList->OMSetRenderTargets(1,
		(camData.allowMSAA > 1) ? &_camera.GetMsaaRtv()->GetCPUDescriptorHandleForHeapStart() : &_camera.GetRtv()->GetCPUDescriptorHandleForHeapStart(),
		true,
		(camData.allowMSAA > 1) ? &_camera.GetMsaaDsv()->GetCPUDescriptorHandleForHeapStart() : &_camera.GetDsv()->GetCPUDescriptorHandleForHeapStart());

	_cmdList->RSSetViewports(1, &_camera.GetViewPort());
	_cmdList->RSSetScissorRects(1, &_camera.GetScissorRect());

	// set pso and topo
	_cmdList->SetPipelineState(_camera.GetDebugMaterial().GetPSO());
	_cmdList->SetGraphicsRootSignature(_camera.GetDebugMaterial().GetRootSignature());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ForwardRenderingPath::DrawScene(Camera _camera, int _frameIdx, int _threadIndex)
{
	FrameResource fr = GraphicManager::Instance().GetFrameResource();
	auto _cmdList = fr.workerGfxList[_threadIndex];
	int threads = GraphicManager::Instance().GetThreadCount() - 1;

	// loop render-queue
	auto queueRenderers = RendererManager::Instance().GetQueueRenderers();
	for (auto const& qr : queueRenderers)
	{
		auto renderers = qr.second;
		int count = (int)renderers.size() / threads + 1;
		int start = _threadIndex * count;

		for (int i = start; i <= start + count; i++)
		{
			if (i >= (int)renderers.size())
			{
				continue;
			}

			auto const r = renderers[i];
			if (!r.cache->GetVisible())
			{
				continue;
			}

			Mesh* m = r.cache->GetMesh();
			if (m == nullptr)
			{
				continue;
			}

			// bind mesh
			_cmdList->IASetVertexBuffers(0, 1, &m->GetVertexBufferView());
			_cmdList->IASetIndexBuffer(&m->GetIndexBufferView());

			// set system constant of renderer
			_cmdList->SetGraphicsRootConstantBufferView(0, r.cache->GetSystemConstantGPU(_frameIdx));

			// draw mesh
			SubMesh sm = m->GetSubMesh(r.submeshIndex);
			_cmdList->DrawIndexedInstanced(sm.IndexCountPerInstance, 1, sm.StartIndexLocation, sm.BaseVertexLocation, 0);

#if defined(GRAPHICTIME)
			GameTimerManager::Instance().gameTime.batchCount[_threadIndex] += 1;
#endif
		}
	}

	// close command list and execute
	LogIfFailedWithoutHR(_cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { _cmdList };
	GraphicManager::Instance().ExecuteCommandList(_countof(cmdsLists), cmdsLists);
}

void ForwardRenderingPath::EndFrame(Camera _camera)
{
	// get frame resource
	FrameResource fr = GraphicManager::Instance().GetFrameResource();
	LogIfFailedWithoutHR(fr.postGfxAllocator->Reset());
	LogIfFailedWithoutHR(fr.postGfxList->Reset(fr.postGfxAllocator, nullptr));

	CameraData camData = _camera.GetCameraData();
	auto _cmdList = fr.postGfxList;

	// transition resource back to common
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition((camData.allowMSAA > 1) ? _camera.GetMsaaRtvSrc(0) : _camera.GetRtvSrc(0)
		, D3D12_RESOURCE_STATE_RENDER_TARGET
		, (camData.allowMSAA > 1) ? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COMMON));

	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition((camData.allowMSAA > 1) ? _camera.GetMsaaDsvSrc() : _camera.GetDsvSrc()
		, D3D12_RESOURCE_STATE_DEPTH_WRITE
		, D3D12_RESOURCE_STATE_COMMON));

	// resolve to non-AA target if MSAA enabled
	if (camData.allowMSAA > 1)
	{
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera.GetRtvSrc(0), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RESOLVE_DEST));
		_cmdList->ResolveSubresource(_camera.GetRtvSrc(0), 0, _camera.GetMsaaRtvSrc(0), 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera.GetRtvSrc(0), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COMMON));
		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera.GetMsaaRtvSrc(0), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_COMMON));
	}

#if defined(GRAPHICTIME)
	// timer end
	_cmdList->EndQuery(GraphicManager::Instance().GetGpuTimeQuery(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
	_cmdList->ResolveQueryData(GraphicManager::Instance().GetGpuTimeQuery(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, GraphicManager::Instance().GetGpuTimeResult(), 0);
#endif

	// close command list and execute
	LogIfFailedWithoutHR(_cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { _cmdList };
	GraphicManager::Instance().ExecuteCommandList(_countof(cmdsLists), cmdsLists);

#if defined(GRAPHICTIME)
	uint64_t* pRes;
	LogIfFailedWithoutHR(GraphicManager::Instance().GetGpuTimeResult()->Map(0, nullptr, reinterpret_cast<void**>(&pRes)));
	uint64_t t1 = pRes[0];
	uint64_t t2 = pRes[1];
	GraphicManager::Instance().GetGpuTimeResult()->Unmap(0, nullptr);

	GameTimerManager::Instance().gameTime.gpuTime = static_cast<float>(t2 - t1) / static_cast<float>(GraphicManager::Instance().GetGpuFreq());
#endif
}
