#include "ForwardRenderingPath.h"
#include "FrameResource.h"
#include "GraphicManager.h"
#include "stdafx.h"
#include "d3dx12.h"

float ForwardRenderingPath::RenderLoop(Camera _camera)
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

	// transition resource state to render target
	fr.mainGraphicList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera.GetRtvSrc(0), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));
	fr.mainGraphicList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera.GetDsvSrc(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// clear render target view and depth view (reversed-z)
	fr.mainGraphicList->ClearRenderTargetView(_camera.GetRtv()->GetCPUDescriptorHandleForHeapStart(), _camera.GetCameraData().clearColor, 0, nullptr);
	fr.mainGraphicList->ClearDepthStencilView(_camera.GetDsv()->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

	// transition resource back to common
	fr.mainGraphicList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera.GetRtvSrc(0), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
	fr.mainGraphicList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_camera.GetDsvSrc(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON));

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
