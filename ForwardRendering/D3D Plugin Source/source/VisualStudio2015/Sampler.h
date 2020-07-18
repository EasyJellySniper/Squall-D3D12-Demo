#pragma once
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

enum TextureWrapMode
{
	Repeat = 0, Clamp, Mirror, MirrorOnce, Border
};

class Sampler
{
public:
	static D3D12_TEXTURE_ADDRESS_MODE UnityWrapModeToNative(TextureWrapMode _wrapMode);
	bool IsSameSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, bool _isCompare = false);
	void CreateSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, bool _isCompare = false);
	void CreateSamplerHeap(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, bool _isCompare = false, bool _isCube = false);

	TextureWrapMode GetWrapU();
	TextureWrapMode GetWrapV();
	TextureWrapMode GetWrapW();
	int GetAnisoLevel();
	ID3D12DescriptorHeap* GetSamplerHeap();

	bool IsCompareSampler();
	void Release();

private:
	TextureWrapMode wrapU;
	TextureWrapMode wrapV;
	TextureWrapMode wrapW;
	int anisoLevel;
	bool isCompare;

	ComPtr<ID3D12DescriptorHeap> samplerHeap = nullptr;
};