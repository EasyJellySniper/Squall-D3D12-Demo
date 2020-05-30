#include "Sampler.h"
#include "GraphicManager.h"

D3D12_TEXTURE_ADDRESS_MODE Sampler::UnityWrapModeToNative(TextureWrapMode _wrapMode)
{
	D3D12_TEXTURE_ADDRESS_MODE mode[4] = { D3D12_TEXTURE_ADDRESS_MODE_WRAP ,D3D12_TEXTURE_ADDRESS_MODE_CLAMP ,D3D12_TEXTURE_ADDRESS_MODE_MIRROR ,D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE };

	return mode[(int)_wrapMode];
}

bool Sampler::IsSameSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel)
{
	return (wrapU == _wrapU) && (wrapV == _wrapV) && (wrapW == _wrapW) && (anisoLevel == _anisoLevel);
}

void Sampler::CreateSampler(TextureWrapMode _wrapU, TextureWrapMode _wrapV, TextureWrapMode _wrapW, int _anisoLevel)
{
	wrapU = _wrapU;
	wrapV = _wrapV;
	wrapW = _wrapW;
	anisoLevel = _anisoLevel;
}

TextureWrapMode Sampler::GetWrapU()
{
	return wrapU;
}

TextureWrapMode Sampler::GetWrapV()
{
	return wrapV;
}

TextureWrapMode Sampler::GetWrapW()
{
	return wrapW;
}

int Sampler::GetAnisoLevel()
{
	return anisoLevel;
}
