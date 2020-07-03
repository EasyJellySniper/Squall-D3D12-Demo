#pragma once
#include "stdafx.h"
#include <DirectXMath.h>
using namespace DirectX;
#include "FrameResource.h"

enum LightType
{
	Spot,
	Directional,
	Point,
};

struct SqLightData
{
	XMFLOAT4 color;
	XMFLOAT4 worldPos;
	int type;
	float intensity;

	// padding for 16-bytes alignment
	XMFLOAT2 padding;
};

class Light
{
public:
	void Init(int _instanceID, SqLightData _data);
	void Release();
	void SetLightData(SqLightData _data);
	SqLightData GetLightData();

	int GetInstanceID();
	void SetDirty(bool _dirty, int _frameIdx);
	bool IsDirty(int _frameIdx);

private:
	static const int MAX_CASCADE_SHADOW = 8;

	bool isDirty[MAX_FRAME_COUNT];
	int instanceID;
	SqLightData lightDataCPU;

	ID3D12Resource* shadowMap[MAX_CASCADE_SHADOW];
	ComPtr<ID3D12DescriptorHeap> shadowMapDSV[MAX_CASCADE_SHADOW];
	ComPtr<ID3D12DescriptorHeap> shadowMapSRV[MAX_CASCADE_SHADOW];
};