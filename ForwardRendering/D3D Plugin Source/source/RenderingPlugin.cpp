// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"
#include "VisualStudio2015/CameraManager.h"
#include "VisualStudio2015/GraphicManager.h"
#include "VisualStudio2015/GameTime.h"
#include "VisualStudio2015/MeshManager.h"
#include "VisualStudio2015/RendererManager.h"
#include "VisualStudio2015/ResourceManager.h"
#include "VisualStudio2015/LightManager.h"
#include "VisualStudio2015/RayTracingManager.h"

#include <assert.h>

static RenderAPI* s_CurrentAPI = NULL;
// define function like this
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitializeSqGraphic(int _numOfThreads, int _width, int _height)
{
	s_CurrentAPI->CreateResources(_numOfThreads);
	GraphicManager::Instance().SetScreenSize(_width, _height);
	return s_CurrentAPI->CheckDevice();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitRayTracingInstance()
{
	RayTracingManager::Instance().InitRayTracingInstance();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitSqLight(int _numDirLight, int _numPointLight, int _numSpotLight)
{
	LightManager::Instance().Init(_numDirLight, _numPointLight, _numSpotLight);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitRayShadow(void *_src, float _shadowScale)
{
	LightManager::Instance().InitRayShadow(_src, _shadowScale);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitRayReflection(void* _src)
{
	LightManager::Instance().InitRayReflection(_src);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitRayAmbient(void* _src, void* _noiseSrc)
{
	LightManager::Instance().InitRayAmbient(_src, _noiseSrc);
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
	, XMFLOAT4X4 _invView, XMFLOAT4X4 _invProj, XMFLOAT3 _position, XMFLOAT3 _direction, float _far, float _near)
{
	Camera* c = CameraManager::Instance().GetCamera(_instanceID);
	if (c != nullptr)
	{
		c->SetViewProj(_view, _proj, _projCulling, _invView, _invProj, _position, _direction, _far, _near);
	}
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetViewPortScissorRect(int _instanceID, D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect)
{
	Camera* c = CameraManager::Instance().GetCamera(_instanceID);
	if (c != nullptr)
	{
		c->SetViewPortScissorRect(_viewPort, _scissorRect);
	}
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetWorldMatrix(int _instanceID, XMFLOAT4X4 _world)
{
	RendererManager::Instance().SetWorldMatrix(_instanceID, _world);
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeMesh(int _instanceID, MeshData _MeshData)
{
	return MeshManager::Instance().AddMesh(_instanceID, _MeshData);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeRenderer(int _instanceID, int _meshInstanceID, bool _isDynamic)
{
	return RendererManager::Instance().AddRenderer(_instanceID, _meshInstanceID, _isDynamic);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetNativeRendererActive(int _id, bool _active)
{
	RendererManager::Instance().SetNativeRendererActive(_id, _active);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeMaterial(int _nRendererId, int _matInstanceId, int _queue, int _cullMode, int _srcBlend, int _dstBlend
	, char* _nativeShader, int _numMacro, char** _macro)
{
	Material* mat = MaterialManager::Instance().AddMaterial(_matInstanceId, _queue, _cullMode, _srcBlend, _dstBlend, _nativeShader, _numMacro, _macro);
	RendererManager::Instance().AddCreatedMaterial(_nRendererId, mat);
}

extern "C" void  UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateLocalBound(int _instanceID, float _x, float _y, float _z, float _ex, float _ey, float _ez)
{
	RendererManager::Instance().UpdateLocalBound(_instanceID, _x, _y, _z, _ex, _ey, _ez);
}

extern "C" void  UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateNativeMaterialProp(int _instanceID, UINT _byteSize, void *_data)
{
	MaterialManager::Instance().UpdateMaterialProp(_instanceID, _byteSize, _data);
}

extern "C" int  UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeTexture(int _instanceID, void *_data)
{
	return ResourceManager::Instance().AddNativeTexture(_instanceID, _data, TextureInfo(false, false, false, false, false));
}

extern "C" int  UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddNativeSampler(TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel)
{
	return ResourceManager::Instance().AddNativeSampler(wrapU, wrapV, wrapW, _anisoLevel, D3D12_FILTER_ANISOTROPIC);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetRenderMode(int _instance, int _mode)
{
	Camera* c = CameraManager::Instance().GetCamera(_instance);
	if (c != nullptr)
	{
		c->SetRenderMode(_mode);
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

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetPCFKernel(int _kernel)
{
	LightManager::Instance().SetPCFKernel(_kernel);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetAmbientLight(XMFLOAT4 _ag, XMFLOAT4 _as, float _skyIntensity)
{
	LightManager::Instance().SetAmbientLight(_ag, _as,_skyIntensity);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetSkybox(void *_skybox, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel, int _meshId)
{
	LightManager::Instance().SetSkybox(_skybox, wrapU, wrapV, wrapW, _anisoLevel, _meshId);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetSkyboxWorld(XMFLOAT4X4 _world)
{
	LightManager::Instance().SetSkyWorld(_world);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitInstanceRendering()
{
	RendererManager::Instance().InitInstanceRendering();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ResetPipelineState()
{
	Camera* c = CameraManager::Instance().GetCamera();

	if (c != nullptr)
	{
		MaterialManager::Instance().GetMaterialCount();
		MaterialManager::Instance().ResetNativeMaterial(c);
	}
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetReflectionData(ReflectionConst _rd)
{
	LightManager::Instance().SetReflectionData(_rd);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetAmbientData(AmbientConstant _ac)
{
	LightManager::Instance().SetAmbientData(_ac);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateRayTracingRange(float _range)
{
	RayTracingManager::Instance().UpdateRayTracingRange(_range);
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

