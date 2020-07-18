#pragma once
#include "RenderingPath.h"
#include "Camera.h"
#include <DirectXMath.h>
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "RendererManager.h"
#include "Light.h"
using namespace Microsoft;

enum WorkerType
{
	Culling = 0,
	Upload,
	PrePassRendering,
	ShadowCulling,
	ShadowRendering,
	OpaqueRendering,
	CutoffRendering,
	TransparentRendering,
};

class ForwardRenderingPath
{
public:
	ForwardRenderingPath(const ForwardRenderingPath&) = delete;
	ForwardRenderingPath(ForwardRenderingPath&&) = delete;
	ForwardRenderingPath& operator=(const ForwardRenderingPath&) = delete;
	ForwardRenderingPath& operator=(ForwardRenderingPath&&) = delete;

	static ForwardRenderingPath& Instance()
	{
		static ForwardRenderingPath instance;
		return instance;
	}

	ForwardRenderingPath() {}
	~ForwardRenderingPath() {}

	void CullingWork(Camera* _camera);
	void SortingWork(Camera* _camera);
	void RenderLoop(Camera* _camera, int _frameIdx);
	void WorkerThread(int _threadIndex);
private:
	void WakeAndWaitWorker();
	void FrustumCulling(int _threadIndex);
	void ShadowCulling(Light* _light, int _cascade, int _threadIndex);
	void BeginFrame(Camera* _camera);
	void ClearCamera(ID3D12GraphicsCommandList *_cmdList, Camera* _camera);
	void ClearLight(ID3D12GraphicsCommandList* _cmdList);
	void UploadWork(Camera* _camera);
	void PrePassWork(Camera* _camera);
	void ShadowWork();
	void BindShadowState(Light *_light, int _cascade, int _threadIndex);
	void BindForwardState(Camera* _camera, int _frameIdx, int _threadIndex);
	void BindDepthObject(ID3D12GraphicsCommandList* _cmdList, Camera* _camera, int _queue, Renderer* _renderer, Material* _mat, Mesh* _mesh, int _frameIdx);
	void BindShadowObject(ID3D12GraphicsCommandList* _cmdList, Light* _light, int _queue, Renderer* _renderer, Material* _mat, Mesh* _mesh, int _frameIdx);
	void BindForwardObject(ID3D12GraphicsCommandList *_cmdList, Renderer *_renderer, Material *_mat, Mesh *_mesh, int _frameIdx);
	void DrawWireFrame(Camera* _camera, int _frameIdx, int _threadIndex);
	void DrawOpaqueDepth(Camera* _camera, int _frameIdx, int _threadIndex);
	void DrawTransparentDepth(ID3D12GraphicsCommandList* _cmdList, Camera* _camera, int _frameIdx);
	void DrawShadowPass(Light* _light, int _cascade, int _frameIdx, int _threadIndex);
	void DrawOpaquePass(Camera* _camera, int _frameIdx, int _threadIndex, bool _cutout = false);
	void DrawCutoutPass(Camera* _camera, int _frameIdx, int _threadIndex);
	void DrawTransparentPass(Camera* _camera, int _frameIdx);
	void DrawSubmesh(ID3D12GraphicsCommandList* _cmdList, Mesh* _mesh, int _subIndex);
	void EndFrame(Camera* _camera);
	void ResolveColorBuffer(ID3D12GraphicsCommandList *_cmdList, Camera* _camera);
	void ResolveDepthBuffer(ID3D12GraphicsCommandList *_cmdList, Camera* _camera);
	void CopyDebugDepth(ID3D12GraphicsCommandList* _cmdList, Camera* _camera);
	void CollectShadow(Light* _light, int _id);
	bool ValidRenderer(int _index, vector<QueueRenderer> _renderers);
	void ExecuteCmdList(ID3D12GraphicsCommandList* _cmdList);

	Camera* targetCam;
	Light* currLight;
	WorkerType workerType;
	int frameIndex;
	int cascadeIndex;
	FrameResource *currFrameResource;
	int numWorkerThreads;
};