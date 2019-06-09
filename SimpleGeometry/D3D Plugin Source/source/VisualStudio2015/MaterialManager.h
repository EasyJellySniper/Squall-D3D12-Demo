#pragma once
#include "Material.h"

class MaterialManager
{
public:
	MaterialManager(const MaterialManager&) = delete;
	MaterialManager(MaterialManager&&) = delete;
	MaterialManager& operator=(const MaterialManager&) = delete;
	MaterialManager& operator=(MaterialManager&&) = delete;

	static MaterialManager& Instance()
	{
		static MaterialManager instance;
		return instance;
	}

	MaterialManager() {}
	~MaterialManager() {}

	void Release();

private:
	
};