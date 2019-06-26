#include "ForwardRenderingPath.h"
#include "FrameResource.h"
#include "GraphicManager.h"
#include "ShaderManager.h"
#include "RendererManager.h"
#include "stdafx.h"
#include "d3dx12.h"

float ForwardRenderingPath::RenderLoop(Camera _camera, int _frameIdx)
{
	// get frame resource
	FrameResource fr = GraphicManager::Instance().GetFrameResource();

	// reset graphic allocator & command list
	LogIfFailedWithoutHR(fr.mainGraphicAllocator->Reset());
	LogIfFailedWithoutHR(fr.mainGraphicList->Reset(fr.mainGraphicAllocator, nullptr));

#if defined(GRAPHICTIME)
	// timer begin
	fr.mainGraphicList->EndQuery(GraphicManager::Instance().GetGpuTimeQuery(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
#endif

	BeginFrame(_camera, fr.mainGraphicList);
	DrawScene(_camera, fr.mainGraphicList, _frameIdx);
	EndFrame(_camera, fr.mainGraphicList);

#if defined(GRAPHICTIME)
	// timer end
	fr.mainGraphicList->EndQuery(GraphicManager::Instance().GetGpuTimeQuery(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
	fr.mainGraphicList->ResolveQueryData(GraphicManager::Instance().GetGpuTimeQuery(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, GraphicManager::Instance().GetGpuTimeResult(), 0);
#endif

	// close command list and execute
	LogIfFailedWithoutHR(fr.mainGraphicList->Close());

	ID3D12CommandList* cmdsLists[] = { fr.mainGraphicList };
	fr.mainGraphicQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

#if defined(GRAPHICTIME)
	uint64_t *pRes;
	LogIfFailedWithoutHR(GraphicManager::Instance().GetGpuTimeResult()->Map(0, nullptr, reinterpret_cast<void**>(&pRes)));
	uint64_t t1 = pRes[0];
	uint64_t t2 = pRes[1];
	GraphicManager::Instance().GetGpuTimeResult()->Unmap(0, nullptr);

	return static_cast<float>(t2 - t1) / static_cast<float>(GraphicManager::Instance().GetGpuFreq());
#endif

	return 0.0f;
}

void ForwardRenderingPath::BeginFrame(Camera _camera, ID3D12GraphicsCommandList * _cmdList)
{
	CameraData camData = _camera.GetCameraData();

	// set view port
	_cmdList->RSSetViewports(1, &_camera.GetViewPort());
	_cmdList->RSSetScissorRects(1, &_camera.GetScissorRect());

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

	_cmdList->OMSetRenderTargets(1,
		(camData.allowMSAA > 1) ? &_camera.GetMsaaRtv()->GetCPUDescriptorHandleForHeapStart() : &_camera.GetRtv()->GetCPUDescriptorHandleForHeapStart(),
		true,
		(camData.allowMSAA > 1) ? &_camera.GetMsaaDsv()->GetCPUDescriptorHandleForHeapStart() : &_camera.GetDsv()->GetCPUDescriptorHandleForHeapStart());

	// set root signature
	_cmdList->SetGraphicsRootSignature(ShaderManager::Instance().GetDefaultRS());
}

void ForwardRenderingPath::DrawScene(Camera _camera, ID3D12GraphicsCommandList * _cmdList, int _frameIdx)
{
	// update camera constant
	auto renderers = RendererManager::Instance().GetRenderers();

	for (auto &r : renderers)
	{
		Mesh *m = r.second->GetMesh();
		if (m == nullptr)
		{
			continue;
		}

		XMFLOAT4X4 world = m->GetWorld();
		XMFLOAT4X4 view = _camera.GetViewMatrix();
		XMFLOAT4X4 proj = _camera.GetProjMatrix();

		SystemConstant sc;
		XMStoreFloat4x4(&sc.sqMatrixMvp, XMLoadFloat4x4(&world) * XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj));
		r.second->UpdateSystemConstant(sc, _frameIdx);
	}

	// set pso and topo
	_cmdList->SetPipelineState(_camera.GetDebugMaterial().GetPSO());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//// render mesh
	for (auto &r : renderers)
	{
		Mesh *m = r.second->GetMesh();
		if (m == nullptr)
		{
			continue;
		}

		_cmdList->IASetVertexBuffers(0, 1, &m->GetVertexBufferView());
		_cmdList->IASetIndexBuffer(&m->GetIndexBufferView());

		auto submeshes = m->GetSubmeshes();
		for (size_t i = 0; i < submeshes.size(); i++)
		{
			// set constant
			_cmdList->SetGraphicsRootConstantBufferView(0, r.second->GetSystemConstantGPU(_frameIdx));

			// draw mesh
			SubMesh sm = submeshes[i];
			_cmdList->DrawIndexedInstanced(sm.IndexCountPerInstance, 1, sm.StartIndexLocation, sm.BaseVertexLocation, 0);
		}
	}
}

void ForwardRenderingPath::EndFrame(Camera _camera, ID3D12GraphicsCommandList * _cmdList)
{
	CameraData camData = _camera.GetCameraData();

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
}
