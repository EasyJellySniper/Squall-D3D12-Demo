#pragma once
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
	void BeginFrame(Camera* _camera);
	void UploadWork(Camera* _camera);
	void PrePassWork(Camera* _camera);
	void BindForwardState(Camera* _camera, int _threadIndex);
	void BindDepthObject(ID3D12GraphicsCommandList* _cmdList, Camera* _camera, int _queue, Renderer* _renderer, Material* _mat, Mesh* _mesh, D3D12_GPU_VIRTUAL_ADDRESS _instanceData);
	void BindForwardObject(ID3D12GraphicsCommandList *_cmdList, Renderer *_renderer, Material *_mat, Mesh *_mesh, D3D12_GPU_VIRTUAL_ADDRESS _instanceData);
	void DrawWireFrame(Camera* _camera, int _threadIndex);
	void DrawOpaqueNormalDepth(Camera* _camera, int _threadIndex);
	void DrawTransparentNormalDepth(ID3D12GraphicsCommandList* _cmdList, Camera* _camera);
	void DrawOpaquePass(Camera* _camera, int _threadIndex, bool _cutout = false);
	void DrawCutoutPass(Camera* _camera, int _threadIndex);
	void DrawSkyboxPass(Camera* _camera);
	void DrawTransparentPass(Camera* _camera);
	void EndFrame(Camera* _camera);

	Camera* targetCam;
	Light* currLight;
	WorkerType workerType;
	int frameIndex;
	int cascadeIndex;
	FrameResource *currFrameResource;
	int numWorkerThreads;
};