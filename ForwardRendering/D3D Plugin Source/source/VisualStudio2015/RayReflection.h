#pragma once
#include "Material.h"

class RayReflection
{
public:
	void Init(ID3D12Resource* _rayReflection);
	void Release();

private:
	ID3D12Resource* rayReflectionSrc;
	Material rayReflectionMat;
	int rayReflectionUav;
	int rayReflectionSrv;
};