#pragma once
#include "stdafx.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
using namespace Microsoft::WRL;
#include "Texture.h"
#include "Sampler.h"

struct DescriptorHeapData
{
	size_t uniqueSrvID;
	size_t uniqueUavID;
	int uav;
	int srv;
	int sampler;
};

class ResourceManager
{
public:
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager(ResourceManager&&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;
	ResourceManager& operator=(ResourceManager&&) = delete;

	static ResourceManager& Instance()
	{
		static ResourceManager instance;
		return instance;
	}

	ResourceManager() {}
	~ResourceManager() {}

	void Init(ID3D12Device* _device);
	void Release();
	int AddNativeTexture(size_t _texId, void* _texData, TextureInfo _info, bool _uavMipmap = false);
	int UpdateNativeTexture(size_t _texId, void* _texData, TextureInfo _info);
	int AddNativeSampler(TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel, D3D12_FILTER _filter);

	ID3D12DescriptorHeap* GetTexHeap();
	ID3D12DescriptorHeap* GetSamplerHeap();

	D3D12_GPU_DESCRIPTOR_HANDLE GetTexHandle(int _id);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerHandle(int _id);

	ID3D12Heap* RequestHeap(D3D12_RESOURCE_DESC _desc);

private:
	void EnlargeTexDescriptorHeap();
	void EnlargeSamplerDescriptorHeap();
	void AddTexToHeap(int _index, Texture _texture, int _mipSlice = 0);
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

	// heap for shared
	ComPtr<ID3D12Heap> heapPool;
	UINT64 prevHeapSize;
};