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
	bool IsSameSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, D3D12_FILTER _filter);
	void CreateSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel, D3D12_FILTER _filter);

	TextureWrapMode GetWrapU();
	TextureWrapMode GetWrapV();
	TextureWrapMode GetWrapW();
	int GetAnisoLevel();
	D3D12_FILTER GetFilter();

private:
	TextureWrapMode wrapU;
	TextureWrapMode wrapV;
	TextureWrapMode wrapW;
	int anisoLevel;
	D3D12_FILTER filter;
};