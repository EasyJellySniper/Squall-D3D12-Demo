#pragma once
#include "stdafx.h"
#include <DirectXMath.h>
using namespace DirectX;
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "RenderTexture.h"
const int MAX_CASCADE_SHADOW = 4;

enum LightType
{
	Spot,
	Directional,
	Point,
};

struct SqLightData
{
	XMFLOAT4X4 shadowMatrix[MAX_CASCADE_SHADOW];
	XMFLOAT4 color;
	XMFLOAT4 worldPos;
	int type;
	float intensity;
	int numCascade;

	// padding for 16-bytes alignment
	float padding;
};

class Light
{
public:
	void Init(int _instanceID, SqLightData _data);
	void InitNativeShadows(int _numCascade, void** _shadowMapRaw);
	void Release();
	void SetLightData(SqLightData _data, bool _forShadow = false);
	void SetViewPortScissorRect(D3D12_VIEWPORT _viewPort, D3D12_RECT _scissorRect);

	SqLightData *GetLightData();
	int GetInstanceID();
	void SetDirty(bool _dirty, int _frameIdx);
	bool IsDirty(int _frameIdx);
	void SetShadowDirty(bool _dirty, int _frameIdx);
	bool IsShadowDirty(int _frameIdx);
	bool HasShadow();
	ID3D12Resource* GetShadowDsvSrc(int _cascade);
	D3D12_CPU_DESCRIPTOR_HANDLE GetShadowDsv(int _cascade);
	D3D12_VIEWPORT GetViewPort();
	D3D12_RECT GetScissorRect();

	void UploadLightConstant(LightConstant lc, int _cascade, int _frameIdx);
	D3D12_GPU_VIRTUAL_ADDRESS GetLightConstantGPU(int _cascade, int _frameIdx);

private:
	bool isDirty[MAX_FRAME_COUNT];
	bool isShadowDirty[MAX_FRAME_COUNT];
	bool hasShadow;
	int instanceID;
	SqLightData lightDataCPU;

	int numCascade = 1;
	shared_ptr<RenderTexture> shadowRT;
	D3D12_VIEWPORT shadowViewPort;
	D3D12_RECT shadowScissorRect;

	shared_ptr<UploadBuffer<LightConstant>> lightConstant[MAX_FRAME_COUNT];
};