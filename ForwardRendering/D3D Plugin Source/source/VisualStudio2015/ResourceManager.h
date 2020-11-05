#pragma once
#include "stdafx.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
using namespace Microsoft::WRL;
#include "Texture.h"
#include "Sampler.h"

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

struct DescriptorHeapData
{
public:
	void AddUav(ID3D12Resource* _src, TextureInfo _info, bool _uavMipmap = false)
	{
		uniqueUavID = GetUniqueID();

		if (_src != nullptr)
			uav = ResourceManager::Instance().AddNativeTexture(uniqueUavID, _src, _info, _uavMipmap);
	}

	void AddSrv(ID3D12Resource* _src, TextureInfo _info)
	{
		uniqueSrvID = GetUniqueID();

		if (_src != nullptr)
			srv = ResourceManager::Instance().AddNativeTexture(uniqueSrvID, _src, _info);
	}

	void UpdateUav(ID3D12Resource* _src, TextureInfo _info)
	{
		if (_src != nullptr)
			uav = ResourceManager::Instance().UpdateNativeTexture(uniqueUavID, _src, _info);
	}

	void UpdateSrv(ID3D12Resource* _src, TextureInfo _info)
	{
		if (_src != nullptr)
			srv = ResourceManager::Instance().UpdateNativeTexture(uniqueSrvID, _src, _info);
	}

	void AddSampler(TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW, int _anisoLevel, D3D12_FILTER _filter)
	{
		sampler = ResourceManager::Instance().AddNativeSampler(wrapU, wrapV, wrapW, _anisoLevel, _filter);
	}

	int Uav()
	{
		return uav;
	}

	int Srv()
	{
		return srv;
	}

	int Sampler()
	{
		return sampler;
	}

private:
	size_t uniqueSrvID;
	size_t uniqueUavID;

	int uav;
	int srv;
	int sampler;
};