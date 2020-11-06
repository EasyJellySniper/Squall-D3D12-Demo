#pragma once
#include "../Material.h"

class FXAA
{
public:
	static void Init();
	static void Release();

private:
	static Material fxaaComputeMat;
};