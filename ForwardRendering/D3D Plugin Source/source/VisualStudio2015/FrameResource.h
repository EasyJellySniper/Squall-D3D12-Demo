#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
using namespace Microsoft::WRL;
using namespace DirectX;

const static int MAX_FRAME_COUNT = 2;
const static int MAX_WORKER_THREAD_COUNT = 8;

struct FrameResource
{
	ID3D12CommandAllocator* mainGfxAllocator;
	ID3D12GraphicsCommandList* mainGfxList;
	ID3D12CommandAllocator *workerGfxAlloc[MAX_WORKER_THREAD_COUNT];
	ID3D12GraphicsCommandList *workerGfxList[MAX_WORKER_THREAD_COUNT];
	int currFrameIndex;
};

struct ObjectConstant
{
	XMFLOAT4X4 sqMatrixWorld;
};

struct SystemConstant
{
	XMFLOAT4X4 sqMatrixViewProj;
	XMFLOAT4X4 sqMatrixInvViewProj;
	XMFLOAT4X4 sqMatrixInvProj;
	XMFLOAT4X4 sqMatrixView;
	XMFLOAT4 ambientGround;
	XMFLOAT4 ambientSky;
	XMFLOAT4 cameraPos;
	XMFLOAT4 screenSize;
	float farZ;
	float nearZ;
	float skyIntensity;
	int numDirLight;
	int numPointLight;
	int numSpotLight;
	int maxPointLight;
	int collectShadowIndex;
	int collectShadowSampler;
	int rayIndex;
	int pcfIndex;
	int msaaCount;
	int depthIndex;
	int transDepthIndex;
	int colorRTIndex;
	int normalRTIndex;
	int tileCountX;
	int tileCountY;
	int reflectionRTIndex;
	int transReflectionRTIndex;
	int linearSampler;
};