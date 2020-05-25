#pragma once
#include "RenderingPath.h"
#include "Camera.h"
#include <DirectXMath.h>
#include "UploadBuffer.h"
#include "FrameResource.h"
using namespace Microsoft;

enum WorkerType
{
	Culling = 0,
	Rendering
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

	void CullingWork(Camera _camera);
	void RenderLoop(Camera _camera, int _frameIdx);
	void WorkerThread(int _threadIndex);
private:
	void BeginFrame(Camera _camera);
	void DrawScene(Camera _camera, int _frameIdx);
	void EndFrame(Camera _camera);

	Camera targetCam;
	WorkerType workerType;
};