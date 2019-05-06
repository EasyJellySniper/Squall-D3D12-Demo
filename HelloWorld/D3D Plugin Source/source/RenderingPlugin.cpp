// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"
#include "VisualStudio2015/CameraManager.h"
#include "VisualStudio2015/GraphicManager.h"
#include "VisualStudio2015/GameTime.h"

#include <assert.h>

static RenderAPI* s_CurrentAPI = NULL;
// define function like this
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitializeSqGraphic(int _numOfThreads)
{
	s_CurrentAPI->CreateResources(_numOfThreads);
	return s_CurrentAPI->CheckDevice();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleaseSqGraphic()
{
	s_CurrentAPI->ReleaseResources();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateSqGraphic()
{
	s_CurrentAPI->OnUpdate();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RenderSqGraphic()
{
	s_CurrentAPI->OnRender();
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderThreadCount()
{
	return s_CurrentAPI->GetRenderThreadCount();
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddCamera(CameraData _cameraData)
{
	return CameraManager::Instance().AddCamera(_cameraData);
}

extern "C" GameTime UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetGameTime()
{
	return GraphicManager::Instance().GetGameTime();
}

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
	
	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

#if UNITY_WEBGL
typedef void	(UNITY_INTERFACE_API * PluginLoadFunc)(IUnityInterfaces* unityInterfaces);
typedef void	(UNITY_INTERFACE_API * PluginUnloadFunc)();

extern "C" void	UnityRegisterRenderingPlugin(PluginLoadFunc loadPlugin, PluginUnloadFunc unloadPlugin);

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterPlugin()
{
	UnityRegisterRenderingPlugin(UnityPluginLoad, UnityPluginUnload);
}
#endif

// --------------------------------------------------------------------------
// GraphicsDeviceEvent
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;


static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		assert(s_CurrentAPI == NULL);
		s_DeviceType = s_Graphics->GetRenderer();
		s_CurrentAPI = CreateRenderAPI(s_DeviceType);
	}

	// Let the implementation process the device related events
	if (s_CurrentAPI)
	{
		s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
	}

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		delete s_CurrentAPI;
		s_CurrentAPI = NULL;
		s_DeviceType = kUnityGfxRendererNull;
	}
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	// Unknown / unsupported graphics device type? Do nothing
	if (s_CurrentAPI == NULL)
		return;
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}

