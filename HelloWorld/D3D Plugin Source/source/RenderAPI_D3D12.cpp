#include "RenderAPI.h"
#include "PlatformBase.h"

// Direct3D 12 implementation of RenderAPI.


#if SUPPORT_D3D12

#include "VisualStudio2015/GraphicManager.h"
#include "VisualStudio2015/CameraManager.h"
#include "Unity/IUnityGraphicsD3D12.h"

class RenderAPI_D3D12 : public RenderAPI
{
public:
	RenderAPI_D3D12(const RenderAPI_D3D12&) = delete;
	RenderAPI_D3D12(RenderAPI_D3D12&&) = delete;
	RenderAPI_D3D12& operator=(const RenderAPI_D3D12&) = delete;
	RenderAPI_D3D12& operator=(RenderAPI_D3D12&&) = delete;

	static RenderAPI_D3D12& Instance()
	{
		static RenderAPI_D3D12 instance;
		return instance;
	}

	RenderAPI_D3D12();
	virtual ~RenderAPI_D3D12() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);
	virtual bool CheckDevice();

	virtual void CreateResources(int _numOfThreads);
	virtual void ReleaseResources();
	virtual int GetRenderThreadCount();

	virtual void OnUpdate();
	virtual void OnRender();

private:
	// d3d12 interfaces
	IUnityGraphicsD3D12v2* s_D3D12;
	ID3D12Device* mainDevice;

	// if init succeed
	bool initSucceed;
};

RenderAPI* CreateRenderAPI_D3D12()
{
	return new RenderAPI_D3D12();
}

const UINT kNodeMask = 0;

RenderAPI_D3D12::RenderAPI_D3D12()
	: s_D3D12(NULL)
{

}

void RenderAPI_D3D12::CreateResources(int _numOfThreads)
{
	mainDevice = s_D3D12->GetDevice();
	if (!mainDevice)
	{
		return;
	}

	initSucceed = GraphicManager::Instance().Initialize(mainDevice, _numOfThreads);
}


void RenderAPI_D3D12::ReleaseResources()
{
	GraphicManager::Instance().Release();
	CameraManager::Instance().Release();
}

int RenderAPI_D3D12::GetRenderThreadCount()
{
	return GraphicManager::Instance().GetThreadCount();
}

void RenderAPI_D3D12::OnUpdate()
{
	GraphicManager::Instance().Update();
}

void RenderAPI_D3D12::OnRender()
{
	GraphicManager::Instance().Render();
}

void RenderAPI_D3D12::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	switch (type)
	{
	case kUnityGfxDeviceEventInitialize:
		s_D3D12 = interfaces->Get<IUnityGraphicsD3D12v2>();

		break;
	case kUnityGfxDeviceEventShutdown:
		ReleaseResources();
		break;
	}
}

bool RenderAPI_D3D12::CheckDevice()
{
	return initSucceed;
}

#endif // #if SUPPORT_D3D12
