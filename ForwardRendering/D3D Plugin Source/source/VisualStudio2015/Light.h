#pragma once
#include "stdafx.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
using namespace DirectX;

#include "UploadBuffer.h"
#include "FrameResource.h"
#include "Texture.h"

enum LightType
{
	Spot,
	Directional,
	Point,
	LightCount
};

struct SqLightData
{
	XMFLOAT4 color;
	XMFLOAT4 worldPos;
	int type;
	float intensity;
	float shadowSize;
	float shadowDistance;
	float range;	// point/spot light range
	float shadowBiasNear;
	float shadowBiasFar;
	float padding;
};

class Light
{
public:
	void Init(int _instanceID, SqLightData _data);
	void Release();
	void SetLightData(SqLightData _data);

	SqLightData *GetLightData();
	int GetInstanceID();
	void SetDirty(bool _dirty, int _frameIdx);
	bool IsDirty(int _frameIdx);

private:
	bool isDirty[MAX_FRAME_COUNT];
	int instanceID;

	SqLightData lightDataCPU;
};