// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"
#include "VisualStudio2015/CameraManager.h"
#include "VisualStudio2015/GraphicManager.h"
#include "VisualStudio2015/GameTime.h"
#include "VisualStudio2015/MeshManager.h"
#include "VisualStudio2015/RendererManager.h"
#include "VisualStudio2015/TextureManager.h"
#include "VisualStudio2015/LightManager.h"

#include <assert.h>

static RenderAPI* s_CurrentAPI = NULL;
// define function like this
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitializeSqGraphic(int _numOfThreads)
{
	s_CurrentAPI->CreateResources(_numOfThreads);
	return s_CurrentAPI->CheckDevice();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitSqLight(int _numDirLight, int _numPointLight, int _numSpotLight, void *_opaqueShadows, int _texId)
{
	LightManager::Instance().Init(_numDirLight, _numPointLight, _numSpotLight, _opaqueShadows, _texId);
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

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RemoveCamera(int _instanceID)
{
	CameraManager::Instance().RemoveCamera(_instanceID);
} 

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetViewProjMatrix(int _instanceID, XMFLOAT4X4 _view, XMFLOAT4X4 _proj, XMFLOAT4X4 _projCulling
	, XMFLOAT4X4 _invView, XMFLOAT4X4 _invProj, XMFLOAT3 _position, float _far, float _near)
{
	CameraManager::Instance().SetViewProjMatrix(_instanceID, _view, _proj, _projCulling, _invView, _invProj, _position, _far, _near);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetViewPortScissorRect(int _instanceID, D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect)
{
	CameraManager::Instance().SetViewPortScissorRect(_instanceID, _viewPort, _scissorRect);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetWorldMatrix(int _instanceID, XMFLOAT4X4 _world)
{
	RendererManager::Instance().SetWorldMatrix(_instanceID, _world);
}

extern "C" GameTime UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetGameTime()
{
	return GraphicManager::Instance().GetGameTime();
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeMesh(int _instanceID, MeshData _MeshData)
{
	return MeshManager::Instance().AddMesh(_instanceID, _MeshData);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeRenderer(int _instanceID, int _meshInstanceID)
{
	return RendererManager::Instance().AddRenderer(_instanceID, _meshInstanceID);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetNativeRendererActive(int _id, bool _active)
{
	RendererManager::Instance().SetNativeRendererActive(_id, _active);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeMaterial(int _nRendererId, int _matInstanceId, int _queue, int _cullMode, int _srcBlend, int _dstBlend
	, char* _nativeShader, int _numMacro, char** _macro)
{
	RendererManager::Instance().AddMaterial(_nRendererId, _matInstanceId, _queue, _cullMode, _srcBlend, _dstBlend, _nativeShader, _numMacro, _macro);
}

extern "C" void  UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateRendererBound(int _instanceID, float _x, float _y, float _z, float _ex, float _ey, float _ez)
{
	RendererManager::Instance().UpdateRendererBound(_instanceID, _x, _y, _z, _ex, _ey, _ez);
}

extern "C" void  UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeMaterialProp(int _instanceID, int _matId, UINT _byteSize, void *_data)
{
	RendererManager::Instance().AddNativeMaterialProp(_instanceID, _matId, _byteSize, _data);
}

extern "C" int  UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeTexture(int _instanceID, void *_data)
{
	return TextureManager::Instance().AddNativeTexture(_instanceID, _data);
}

extern "C" int  UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeSampler(TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel)
{
	return TextureManager::Instance().AddNativeSampler(wrapU, wrapV, wrapW, _anisoLevel);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetRenderMode(int _instance, int _mode)
{
	Camera* c = CameraManager::Instance().GetCamera(_instance);
	if (c != nullptr)
	{
		c->SetRenderMode(_mode);
	}
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetDebugDepth(int _id, void* _debugDepth)
{
	Camera* c = CameraManager::Instance().GetCamera(_id);
	if (c != nullptr)
	{
		c->CopyDepth(_debugDepth);
	}
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeLight(int _instanceID, SqLightData _sqLightData)
{
	return LightManager::Instance().AddNativeLight(_instanceID, _sqLightData);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateNativeLight(int _nativeID, SqLightData _sqLightData)
{
	LightManager::Instance().UpdateNativeLight(_nativeID, _sqLightData);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateNativeShadow(int _nativeID, SqLightData _sqLightData)
{
	LightManager::Instance().UpdateNativeShadow(_nativeID, _sqLightData);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitNativeShadows(int _nativeID, int _numCascade, void** _shadowMapRaw)
{
	LightManager::Instance().InitNativeShadows(_nativeID, _numCascade, _shadowMapRaw);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetShadowViewPortScissorRect(int _nativeId, D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect)
{
	LightManager::Instance().SetViewPortScissorRect(_nativeId, _viewPort, _scissorRect);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetShadowFrustum(int _id, XMFLOAT4X4 _view, XMFLOAT4X4 _projCulling, int _cascade)
{
	LightManager::Instance().SetShadowFrustum(_id, _view, _projCulling, _cascade);
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

