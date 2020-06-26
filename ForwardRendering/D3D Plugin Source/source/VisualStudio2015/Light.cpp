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

int Light::GetInstanceID()
{
	return instanceID;
}

bool Light::IsDirty()
{
	return isDirty;
}
