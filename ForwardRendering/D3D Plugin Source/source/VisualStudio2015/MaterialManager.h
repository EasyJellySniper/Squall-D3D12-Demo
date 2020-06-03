#pragma once
#include "Shader.h"
#include "Camera.h"
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
	Material CreateMaterialFromShader(Shader *_shader, Camera _camera, D3D12_FILL_MODE _fillMode, D3D12_CULL_MODE _cullMode);
	Material CreateMaterialPost(Shader* _shader, Camera _camera, bool _zWrite);

	Material* AddMaterial(int _matInstanceId, int _renderQueue, int _cullMode);
	void Release();

private:
	unordered_map<int, unique_ptr<Material>> materialTable;
};