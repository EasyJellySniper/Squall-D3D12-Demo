#pragma once
#include "stdafx.h"
#include <DirectXMath.h>
using namespace DirectX;
#include "FrameResource.h"
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
	void SetLightData(SqLightData _data);
	SqLightData GetLightData();

	int GetInstanceID();
	void SetDirty(bool _dirty, int _frameIdx);
	bool IsDirty(int _frameIdx);

private:
	bool isDirty[MAX_FRAME_COUNT];
	int instanceID;
	SqLightData lightDataCPU;

	int numCascade = 1;
	ID3D12Resource* shadowMap[MAX_CASCADE_SHADOW];
	ComPtr<ID3D12DescriptorHeap> shadowMapDSV;
	ComPtr<ID3D12DescriptorHeap> shadowMapSRV;
};