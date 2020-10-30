#pragma once
#include "../Material.h"

class GaussianBlur
{
public:
	static void Init();
	static void Release();

private:
	static Material blurCompute;
};