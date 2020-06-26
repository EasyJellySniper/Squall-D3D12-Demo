#include "Light.h"

void Light::Init(int _instanceID, SqLightData _data)
{
	instanceID = _instanceID;
	lightDataCPU = _data;
	isDirty = false;
}

void Light::SetLightData(SqLightData _data)
{
	lightDataCPU = _data;
	isDirty = true;
}

SqLightData Light::GetLightData()
{
	return lightDataCPU;
}

int Light::GetInstanceID()
{
	return instanceID;
}

void Light::SetDirty(bool _dirty)
{
	isDirty = _dirty;
}

bool Light::IsDirty()
{
	return isDirty;
}
