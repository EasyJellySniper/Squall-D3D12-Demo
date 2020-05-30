#pragma once
#include <d3d12.h>

enum TextureWrapMode
{
	Repeat = 0, Clamp, Mirror, MirrorOnce
};

class Sampler
{
public:
	static D3D12_TEXTURE_ADDRESS_MODE UnityWrapModeToNative(TextureWrapMode _wrapMode);
	bool IsSameSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel);
	void CreateSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel);

	TextureWrapMode GetWrapU();
	TextureWrapMode GetWrapV();
	TextureWrapMode GetWrapW();
	int GetAnisoLevel();

private:
	TextureWrapMode wrapU;
	TextureWrapMode wrapV;
	TextureWrapMode wrapW;
	int anisoLevel;
};