#pragma once
#include "Camera.h"

enum RenderingPathType
{
	Forward = 1,
	Deferred = 3
};

class RenderingPath
{
public:
	virtual float RenderLoop(Camera _camera) = 0;

private:
	RenderingPathType renderingPath;
};