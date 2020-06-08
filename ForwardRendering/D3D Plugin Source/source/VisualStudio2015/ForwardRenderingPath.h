#pragma once
#include "RenderingPath.h"
#include "Camera.h"
#include <DirectXMath.h>
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "RendererManager.h"
using namespace Microsoft;

enum WorkerType
{
	Culling = 0,
	Rendering
};

inline bool FrontToBackRender(QueueRenderer const &i, QueueRenderer const &j)
{
	// sort from low distance to high distance (front to back)
	return (i.zDistanceToCam < j.zDistanceToCam);
}

inline bool BackToFrontRender(QueueRenderer const &i, QueueRenderer const &j)
{
	// sort from high distance to low distance (back to front)
	return (i.zDistanceToCam > j.zDistanceToCam);
}

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

	void CullingWork(Camera _camera);
	void SortingWork(Camera _camera);
	void RenderLoop(Camera _camera, int _frameIdx);
	void WorkerThread(int _threadIndex);
private:
	void FrustumCulling(int _threadIndex);
	void BeginFrame(Camera _camera);
	void UploadSystemConstant(Camera _camera, int _frameIdx, int _threadIndex);
	void BindState(Camera _camera, int _frameIdx, int _threadIndex);
	void DrawWireFrame(Camera _camera, int _frameIdx, int _threadIndex);
	void DrawPrepassDepth(Camera _camera, int _frameIdx, int _threadIndex);
	void EndFrame(Camera _camera);
	void ResolveColorBuffer(ID3D12GraphicsCommandList *_cmdList, Camera _camera);
	void ResolveDepthBuffer(ID3D12GraphicsCommandList *_cmdList, Camera _camera);
	bool ValidRenderer(int _index, vector<QueueRenderer> _renderers);

	Camera targetCam;
	WorkerType workerType;
	int frameIndex;
	FrameResource currFrameResource;
	int numWorkerThreads;
};