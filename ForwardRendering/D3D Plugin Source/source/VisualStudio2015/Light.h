#pragma once
#include "stdafx.h"
#include <DirectXMath.h>
using namespace DirectX;

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

	void SetLightData(SqLightData _data);
	int GetInstanceID();
	bool IsDirty();

private:
	bool isDirty;
	int instanceID;
	SqLightData lightDataCPU;
};