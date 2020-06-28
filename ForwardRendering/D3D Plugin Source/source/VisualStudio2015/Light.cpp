#include "Light.h"

void Light::Init(int _instanceID, SqLightData _data)
{
	instanceID = _instanceID;
	lightDataCPU = _data;

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		isDirty[i] = false;
	}
}

void Light::SetLightData(SqLightData _data)
{
	lightDataCPU = _data;

	for (int i = 0; i < MAX_FRAME_COUNT; i++)
	{
		isDirty[i] = true;
	}
}

SqLightData Light::GetLightData()
{
	return lightDataCPU;
}

int Light::GetInstanceID()
{
	return instanceID;
}

void Light::SetDirty(bool _dirty, int _frameIdx)
{
	isDirty[_frameIdx] = _dirty;
}

bool Light::IsDirty(int _idx)
{
	return isDirty[_idx];
}
