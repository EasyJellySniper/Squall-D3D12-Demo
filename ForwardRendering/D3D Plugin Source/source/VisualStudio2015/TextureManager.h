#pragma once
#include "stdafx.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
using namespace Microsoft::WRL;
#include "Texture.h"
#include "Sampler.h"

class TextureManager
{
public:
	TextureManager(const TextureManager&) = delete;
	TextureManager(TextureManager&&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;
	TextureManager& operator=(TextureManager&&) = delete;

	static TextureManager& Instance()
	{
		static TextureManager instance;
		return instance;
	}

	TextureManager() {}
	~TextureManager() {}

	void Init(ID3D12Device* _device);
	void Release();
	int AddNativeTexture(int _texId, void* _texData);
	int AddNativeSampler(TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel);

private:
	void EnlargeTexDescriptorHeap();
	void EnlargeSamplerDescriptorHeap();
	void AddTexToHeap(int _index, Texture _texture);
	void AddSamplerToHeap(int _index, Sampler _sampler);

	static const int MAX_TEXTURE_NUMBER = 1000;
	static const int MAX_SAMPLER_NUMBER = 100;
	ComPtr<ID3D12DescriptorHeap> texDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> samplerDescriptorHeap = nullptr;
	vector<Texture> textures;
	vector<Sampler> samplers;

	UINT cbvSrvUavDescriptorSize;
	UINT samplerDescriptorSize;
	int texHeapEnlargeCount;
	int samplerHeapEnlargeCount;
};