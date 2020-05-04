#pragma once

#include "Unity/IUnityGraphics.h"

#include <stddef.h>

struct IUnityInterfaces;

class RenderAPI
{
public:
	virtual ~RenderAPI() { }


	// Process general event like initialization, shutdown, device loss/reset etc.
	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) = 0;

	virtual bool CheckDevice() = 0;
	virtual void CreateResources(int _numOfThreads) = 0;
	virtual void ReleaseResources() = 0;
	virtual int GetRenderThreadCount() = 0;

	virtual void OnUpdate() = 0;
	virtual void OnRender() = 0;
};


// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);