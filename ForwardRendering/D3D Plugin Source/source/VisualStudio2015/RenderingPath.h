#pragma once
#include "Camera.h"
#include <DirectXMath.h>
#include "UploadBuffer.h"
#include "FrameResource.h"
using namespace Microsoft;

enum RenderingPathType
{
	Forward = 1,
	Deferred = 3
};

class RenderingPath
{
public:
	virtual void RenderLoop(Camera _camera, int _frameIdx) = 0;

private:
	RenderingPathType renderingPath;
};